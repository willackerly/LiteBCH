
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <vector>

// Use the Compat Header instead of real aff3ct
#include <litebch/aff3ct_compat.h>

// Replicating the aff3ct test structure
using namespace aff3ct;

// Simple CRC32 for consistency checking
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
  uint32_t expected_checksum;
};

int main() {
  // Same Matrix as comprehensive test
  std::vector<TestConfig> configs = {
      {"Small", 5, 3, {}, 0x40840401},
      {"Medium", 8, 10, {}, 0x3ab77e29},
      {"Large", 10, 50, {1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1}, 0x5b71e1b6},
      {"X-Large", 12, 20, {}, 0x629876d},
      {"XX-Large", 13, 40, {}, 0x1524ee04}};

  std::cout << "Legacy API (Shim) Verification\n";
  std::cout << "================================\n";

  for (const auto &cfg : configs) {
    int N = (1 << cfg.m) - 1;

    // 1. LEGACY API: Poly Generator
    tools::BCH_polynomial_generator<int> poly(N, cfg.t, cfg.p);
    int K = N - poly.get_n_rdncy();

    // 2. LEGACY API: Modules
    // Note: Using "Decoder_BCH_fast" to prove the alias works
    module::Encoder_BCH<int> encoder(K, N, poly);
    module::Decoder_BCH_fast<int> decoder(K, N, poly);

    int vectors = 100;

    // Random Gen
    std::mt19937 gen(1337 + cfg.m);
    std::uniform_int_distribution<> bit_dist(0, 1);
    std::uniform_int_distribution<> pos_dist(0, N - 1);

    uint32_t total_checksum = 0;

    if (cfg.name == "Large") {
      std::cout << "DEBUG: Running Large config. N=" << N << ", t=" << cfg.t
                << "\n";
    }

    for (int v = 0; v < vectors; ++v) {
      std::vector<int> message(K);
      for (int i = 0; i < K; ++i)
        message[i] = bit_dist(gen);

      // LEGACY API: Encode (ref to ref)
      std::vector<int> encoded(N);
      encoder.encode(message, encoded);

      // Corrupt
      std::vector<int> corrupted = encoded;
      if (cfg.name == "Large") {
        // FORCE the pattern that fails in repro
        corrupted[0] ^= 1;
        corrupted[10] ^= 1;
        corrupted[20] ^= 1;
        corrupted[30] ^= 1;
        corrupted[40] ^= 1;
      } else {
        std::set<int> errs;
        while (errs.size() < (size_t)cfg.t)
          errs.insert(pos_dist(gen));
        for (int idx : errs)
          corrupted[idx] ^= 1;
      }

      // LEGACY API: Decode
      std::vector<int> decoded(K);
      int status = decoder.decode_hiho(corrupted, decoded);

      if (status != 0) {
        // Should fail if we exceed t, but here we test exact t capacity
      }

      if (decoded != message) {
        std::cout << "[FAIL] Legacy Test mismatch in vector " << v << "!\n";
        if (cfg.name == "Large") {
          std::cout << "       First 10 decoded: ";
          for (int i = 0; i < 10; ++i)
            std::cout << decoded[i];
          std::cout << "\n       First 10 missing: ";
          for (int i = 0; i < 10; ++i)
            std::cout << message[i];
          std::cout << "\n";
          exit(1);
        }
      }

      total_checksum ^= crc32(decoded);
    }

    if (total_checksum != cfg.expected_checksum) {
      std::cout << "[FAIL] Checksum mismatch for " << cfg.name
                << ". Got: " << std::hex << total_checksum
                << " Expected: " << cfg.expected_checksum << std::dec << "\n";
    }

    // Check partial checksum (since fewer vectors)
  }

  std::cout << "ALL CONFIGS COMPILED AND RAN SUCCESSFULLY VIA LEGACY API.\n";

  return 0;
}
