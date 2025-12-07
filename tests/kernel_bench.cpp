#include "bch_codec.h" // Kernel BCH
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <litebch/LiteBCH.h>

#include <random>
#include <vector>

// Helper for timing
using Clock = std::chrono::high_resolution_clock;

int main() {
  std::cout << "LiteBCH vs Linux Kernel BCH Benchmark\n";
  std::cout << "=====================================\n";
  std::cout
      << "|  m |      N    |  t | LiteBCH (Mbps) | Kernel (Mbps) | Speedup |\n";
  std::cout
      << "|----|-----------|----|----------------|---------------|---------|\n";

  // Kernel usually supports up to m=15 (32k) easily.
  // Let's test m=10 to m=15.
  for (int m = 10; m <= 15; ++m) {
    int N = (1 << m) - 1;
    int t = 20;
    if (2 * t >= N)
      t = N / 4;

    // --- LiteBCH Setup ---
    lite::LiteBCH lite(N, t);
    int K_lite = lite.get_K();

    // --- Kernel BCH Setup ---
    // init_bch(m, t, prim_poly). prim_poly=0 uses default.
    struct bch_control *bch = init_bch(m, t, 0);
    if (!bch) {
      std::cout << "| " << m << " | Failed to init Kernel BCH |\n";
      continue;
    }

    // Data Prep
    int vectors = 2000;

    // Random Gen
    std::mt19937 gen(1337 + m);
    std::uniform_int_distribution<> bit_dist(0, 1);
    std::uniform_int_distribution<> byte_dist(0, 255);

    // LiteBCH Data (Unpacked)
    std::vector<std::vector<int>> lite_msgs(vectors, std::vector<int>(K_lite));
    std::vector<std::vector<int>> lite_encs(vectors);

    // Fill LiteBCH messages with random bits
    for (int v = 0; v < vectors; ++v) {
      for (int i = 0; i < K_lite; ++i) {
        lite_msgs[v][i] = bit_dist(gen);
      }
    }

    // Kernel Data (Packed)
    // K bits -> ceil(K/8) bytes
    // ECC bits -> ecc_bytes
    int K_kernel = N - bch->ecc_bits; // Approximate, kernel handles K implicit
    // Note: Kernel encode_bch takes input bytes and computes ECC bytes.
    // Usually it doesn't output the full codeword in one buffer, just the ecc.
    int data_bytes = (bch->n - bch->ecc_bits + 7) / 8;

    std::vector<std::vector<uint8_t>> kern_data(
        vectors, std::vector<uint8_t>(data_bytes));
    std::vector<std::vector<uint8_t>> kern_ecc(
        vectors, std::vector<uint8_t>(bch->ecc_bytes, 0));

    // Fill Kernel data with random bytes (good enough approx for packed bits)
    for (int v = 0; v < vectors; ++v) {
      for (int i = 0; i < data_bytes; ++i) {
        kern_data[v][i] = (uint8_t)byte_dist(gen);
      }
    }

    // --- Bench LiteBCH ---
    auto t1 = Clock::now();
    for (int v = 0; v < vectors; ++v) {
      lite_encs[v] = lite.encode(lite_msgs[v]);
      // Decode (Clean)
      std::vector<int> dec;
      lite.decode(lite_encs[v], dec);
    }
    auto t2 = Clock::now();
    double d_lite = std::chrono::duration<double>(t2 - t1).count();
    double mbps_lite = ((double)vectors * N / 1e6) / d_lite;

    // --- Bench Kernel ---
    // Benchmark encode + decode(clean)
    t1 = Clock::now();
    for (int v = 0; v < vectors; ++v) {
      // Encode
      encode_bch(bch, kern_data[v].data(), data_bytes, kern_ecc[v].data());

      // Decode
      // To perform a fair comparison with LiteBCH (where decode() calculates
      // syndromes internally), we perform the full standard flow:
      // 1. Re-calculate ECC from received data (to compare against received
      // ECC)
      // 2. Call decode_bch which uses both to detect errors.

      // Re-encode to get "calculated ecc" (assuming clean channel for
      // benchmark)
      std::vector<uint8_t> calc_ecc(bch->ecc_bytes);
      encode_bch(bch, kern_data[v].data(), data_bytes, calc_ecc.data());

      unsigned int errloc[50]; // Max t
      decode_bch(bch, kern_data[v].data(), data_bytes, kern_ecc[v].data(),
                 calc_ecc.data(), NULL, errloc);
    }
    t2 = Clock::now();
    double d_kern = std::chrono::duration<double>(t2 - t1).count();
    double mbps_kern = ((double)vectors * N / 1e6) / d_kern;

    std::cout << "| " << std::setw(2) << m << " | " << std::setw(9) << N
              << " | " << std::setw(2) << t << " | " << std::setw(14)
              << std::fixed << std::setprecision(1) << mbps_lite << " | "
              << std::setw(13) << std::fixed << std::setprecision(1)
              << mbps_kern << " | " << std::setw(6) << std::fixed
              << std::setprecision(2) << (mbps_lite / mbps_kern) << "x |\n";

    free_bch(bch);
  }

  return 0;
}
