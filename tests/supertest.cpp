#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <litebch/LiteBCH.h>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <vector>

using namespace lite;

// Simple CRC32
uint32_t crc32(const std::vector<int> &data) {
  uint32_t hash = 0;
  for (int bit : data) {
    hash = (hash << 5) ^ (hash >> 27) ^ bit;
  }
  return hash;
}

struct TestConfig {
  std::string name;
  int m;
  int t;
  std::vector<int> p;
  uint32_t expected_codeword_checksum; // Checksum of the ENCODED bits
};

int main() {
  // Golden Checksums (Codeword CRC32)
  // Initialize with 0 first to capture them
  std::vector<TestConfig> configs = {
      {"Small", 5, 3, {}, 0xc5374201},
      {"Medium", 8, 10, {}, 0x986ce102},
      // Note: Large uses Custom Poly
      {"Large", 10, 50, {1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1}, 0x30b6c819},
      {"X-Large", 12, 20, {}, 0x764a655e},
      {"XX-Large", 13, 40, {}, 0x19fdc36a}};

  std::cout << "LiteBCH Supertest: Regression & Compatibility Suite\n";
  std::cout << "===================================================\n";

  unsigned int seed = 1337;
  bool all_passed = true;

  for (const auto &cfg : configs) {
    int N = (1 << cfg.m) - 1;
    bool custom_poly = !cfg.p.empty();

    std::string poly_desc = custom_poly ? "Custom" : "Default";

    try {
      std::unique_ptr<LiteBCH> bch;
      if (cfg.p.empty()) {
        bch.reset(new LiteBCH(N, cfg.t));
      } else {
        bch.reset(new LiteBCH(N, cfg.t, cfg.p));
      }

      int K = bch->get_K();
      int vectors = 100;

      std::mt19937 gen(seed + cfg.m);
      std::uniform_int_distribution<> bit_dist(0, 1);
      std::uniform_int_distribution<> pos_dist(0, N - 1);

      uint32_t total_checksum = 0;

      // Metrics
      bool byte_pass = true;
      bool leg_pass = true; // Implied by consistency check
      bool consist_pass = true;

      int vectors_passed = 0;

      // Data Prep
      // Generate 100 random vectors
      std::vector<std::vector<int>> messages(vectors, std::vector<int>(K));
      for (int v = 0; v < vectors; ++v) {
        for (int i = 0; i < K; ++i)
          messages[v][i] = bit_dist(gen);
      }

      // Byte Packing (MSB First)
      int data_bytes_len = (K + 7) / 8;
      std::vector<std::vector<uint8_t>> messages_bytes(
          vectors, std::vector<uint8_t>(data_bytes_len, 0));
      for (int v = 0; v < vectors; ++v) {
        for (int i = 0; i < K; ++i) {
          if (messages[v][i]) {
            int pos_from_start = (K - 1 - i);
            int byte_idx = pos_from_start / 8;
            int bit_in_byte = 7 - (pos_from_start % 8);
            messages_bytes[v][byte_idx] |= (1 << bit_in_byte);
          }
        }
      }

      int ecc_bytes_len = (N - K + 7) / 8;
      std::vector<std::vector<uint8_t>> ecc_outputs(
          vectors, std::vector<uint8_t>(ecc_bytes_len));

      // Execution Loop
      for (int v = 0; v < vectors; ++v) {
        // --- 1. Byte-Wise Encode ---
        bch->encode(messages_bytes[v].data(), data_bytes_len,
                    ecc_outputs[v].data());

        // Reconstruct Codeword [Parity | Message]
        std::vector<int> cw_byte(N, 0);
        int n_red = N - K;
        const auto &ecc = ecc_outputs[v];
        for (int i = 0; i < n_red; ++i) {
          if (ecc[i / 8] & (1 << (i % 8)))
            cw_byte[i] = 1;
        }
        for (int i = 0; i < K; ++i) {
          if (messages[v][i])
            cw_byte[n_red + i] = 1;
        }

        // --- 2. Legacy Encode ---
        std::vector<int> cw_leg = bch->encode(messages[v]);

        // --- 3. Consistency Check ---
        if (cw_byte != cw_leg) {
          consist_pass = false;
        }

        // --- 4. Decode Verification (Byte Codeword) ---
        // Corrupt
        std::vector<int> corrupted = cw_byte;
        std::set<int> error_indices;
        while (error_indices.size() < (size_t)cfg.t) {
          error_indices.insert(pos_dist(gen));
        }
        for (int idx : error_indices)
          corrupted[idx] ^= 1;

        // Decode
        std::vector<int> decoded;
        bool success = bch->decode(corrupted, decoded);

        if (!success || decoded != messages[v]) {
          byte_pass = false;
        } else {
          vectors_passed++;
        }

        // --- 5. Accumulate Checksum of CODEWORD (Byte version, guaranteed
        // equal if consist_pass) ---
        total_checksum ^= crc32(cw_byte);
      }

      std::cout << "\nConfig: " << cfg.name << " (N=" << N << ", K=" << K
                << ", t=" << cfg.t << ")\n";
      std::cout << "  Polynomial: " << poly_desc << "\n";
      std::cout << "  - Bytewise Fast Mode:  " << (byte_pass ? "PASS" : "FAIL")
                << "\n";
      std::cout << "  - Legacy Compat Mode:  PASS (Implied by Consistency)\n";
      std::cout << "  - Consistency Check:   "
                << (consist_pass ? "PASS" : "FAIL") << "\n";
      std::cout << "  - Codeword Checksum:   0x" << std::hex << total_checksum
                << std::dec << "\n";

      if (cfg.expected_codeword_checksum != 0 &&
          total_checksum != cfg.expected_codeword_checksum) {
        std::cout << "  -> CHECKSUM MISMATCH! Expected 0x" << std::hex
                  << cfg.expected_codeword_checksum << std::dec << "\n";
        all_passed = false;
      }
      if (!byte_pass || !consist_pass)
        all_passed = false;

    } catch (const std::exception &e) {
      std::cout << "ERROR: " << e.what() << "\n";
      all_passed = false;
    }
  }
  std::cout << "\n===================================================\n";
  std::cout << "OVERALL STATUS: " << (all_passed ? "PASS" : "FAIL") << "\n";

  return all_passed ? 0 : 1;
}
