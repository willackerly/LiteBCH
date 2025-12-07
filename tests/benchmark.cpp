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

// Simple CRC32 for checksumming decoded bits
// (Matches the one used in the old repo to produce the 0x40840401 etc)
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
  std::vector<int> p; // Empty for default
  uint32_t expected_checksum;
};

int main() {
  std::vector<TestConfig> configs = {
      {"Small", 5, 3, {}, 0x10940c23},
      {"Medium", 8, 10, {}, 0xb6d64c48},
      {"Large", 10, 50, {1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1}, 0x76754cec},
      {"X-Large", 12, 20, {}, 0x7aac0868},
      {"XX-Large", 13, 40, {}, 0x3688461c}};

  std::cout << "==============================================================="
               "==========================================================\n";
  std::cout << "| Config   | m  | N    | t  | K    | Vect | Legacy Time | Byte "
               "Time   | Speedup | Legacy Check | Byte Check   | Status |\n";
  std::cout << "|----------|----|------|----|------|------|-------------|------"
               "-------|---------|--------------|--------------|--------|\n";

  unsigned int seed = 1337;

  for (const auto &cfg : configs) {
    int N = (1 << cfg.m) - 1;

    try {
      std::unique_ptr<LiteBCH> bch;
      if (cfg.p.empty()) {
        bch.reset(new LiteBCH(N, cfg.t));
      } else {
        bch.reset(new LiteBCH(N, cfg.t, cfg.p));
      }

      int K = bch->get_K();
      int vectors = 1000;

      // Pre-generate data to exclude RNG from benchmark timing
      std::mt19937 gen(seed +
                       cfg.m); // Keep seed dependence for checksum consistency
      std::uniform_int_distribution<> bit_dist(0, 1);

      std::vector<std::vector<int>> messages(vectors, std::vector<int>(K));
      for (int v = 0; v < vectors; ++v) {
        for (int i = 0; i < K; ++i)
          messages[v][i] = bit_dist(gen);
      }

      // Prepare Byte-Wise Inputs (MSB Packed)
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

      // --- 1. Legacy Benchmark ---
      uint32_t leg_checksum = 0;
      auto start_leg = std::chrono::high_resolution_clock::now();

      for (int v = 0; v < vectors; ++v) {
        // Encode
        std::vector<int> encoded = bch->encode(messages[v]);

        // Checksum (simulate usage)
        leg_checksum ^= crc32(encoded);
      }

      auto end_leg = std::chrono::high_resolution_clock::now();
      double ms_leg =
          std::chrono::duration<double, std::milli>(end_leg - start_leg)
              .count();

      // --- 2. Byte-Wise Benchmark ---
      uint32_t byte_checksum = 0;

      // We need to reconstruct the full bit vector to match CRC32 calculation
      // So we can verify checksum matches Legacy.
      // But for timing, we only measure Encode.

      auto start_byte = std::chrono::high_resolution_clock::now();
      for (int v = 0; v < vectors; ++v) {
        bch->encode(messages_bytes[v].data(), data_bytes_len,
                    ecc_outputs[v].data());
      }
      auto end_byte = std::chrono::high_resolution_clock::now();
      double ms_byte =
          std::chrono::duration<double, std::milli>(end_byte - start_byte)
              .count();

      // Post-Calculate Byte Checksum (outside timing)
      for (int v = 0; v < vectors; ++v) {
        // Reconstruct bits for CRC
        std::vector<int> fast_codeword(N, 0);
        // Msg
        for (int i = 0; i < K; ++i)
          if (messages[v][i])
            fast_codeword[i] = 1;
        // ECC
        int n_red = N - K;
        const auto &ecc = ecc_outputs[v];
        for (int i = 0; i < n_red; ++i) {
          if (ecc[i / 8] & (1 << (i % 8)))
            fast_codeword[K + (n_red - 1 - i)] =
                1; // Reverse fill to match legacy
        }
        byte_checksum ^= crc32(fast_codeword);
      }

      std::string status = "PASS";
      if (leg_checksum != cfg.expected_checksum)
        status = "BAD_LEG_SUM";
      if (byte_checksum != leg_checksum)
        status = "MISMATCH";

      std::cout << "| " << std::left << std::setw(8) << cfg.name << " | "
                << std::setw(2) << cfg.m << " | " << std::setw(4) << N << " | "
                << std::setw(2) << cfg.t << " | " << std::setw(4) << K << " | "
                << std::setw(4) << vectors << " | " << std::setw(11)
                << std::fixed << std::setprecision(2) << ms_leg << " | "
                << std::setw(11) << ms_byte << " | " << std::setw(7)
                << (ms_leg / ms_byte) << "x | 0x" << std::hex << std::setw(8)
                << std::setfill('0') << leg_checksum << "   | 0x"
                << std::setw(8) << byte_checksum << "   | " << std::setw(6)
                << std::setfill(' ') << status << " |\n"
                << std::dec;

    } catch (const std::exception &e) {
      std::cout << "| " << cfg.name << " | ERROR: " << e.what() << "\n";
    }
  }
  std::cout << "==============================================================="
               "==========================================================\n";

  return 0;
}
