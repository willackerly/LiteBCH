#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <litebch/LiteBCH.h>
#include <random>
#include <vector>

using Clock = std::chrono::high_resolution_clock;

int main() {
  std::cout << "LiteBCH Fast (Integrated) Benchmark\n";
  std::cout << "===================================\n";
  std::cout << "|  m |      N    |  t | Std (Mbps) | Fast (Mbps) | Speedup |\n";
  std::cout << "|----|-----------|----|------------|-------------|---------|\n";

  for (int m = 10; m <= 15; ++m) {
    int N = (1 << m) - 1;
    int t = 20;
    if (2 * t >= N)
      t = N / 4;

    // Setup (New: Fast tables init by default)
    lite::LiteBCH bch(N, t);

    int vectors = 5000;
    int K = bch.get_K();
    int data_bytes = (K + 7) / 8;
    int ecc_bytes = bch.ecc_bytes;

    // Random Gen
    std::mt19937 gen(1337 + m);
    std::uniform_int_distribution<> byte_dist(0, 255);

    // Data buffers
    std::vector<uint8_t> input_data(data_bytes);
    for (int i = 0; i < data_bytes; ++i) {
      input_data[i] = (uint8_t)byte_dist(gen);
    }

    std::vector<uint8_t> output_ecc(ecc_bytes);

    // 1. Std Encode
    auto t1 = Clock::now();

    // Include conversion cost to represent real-world penalties of standard
    // approach
    std::vector<int> bits_in(K, 1);
    for (int v = 0; v < vectors; ++v) {
      auto enc = bch.encode(bits_in);
    }
    auto t2 = Clock::now();
    double d_std = std::chrono::duration<double>(t2 - t1).count();
    double mbps_std = ((double)vectors * N / 1e6) / d_std;

    // 2. Fast Encode
    t1 = Clock::now();
    for (int v = 0; v < vectors; ++v) {
      bch.encode(input_data.data(), data_bytes, output_ecc.data());
    }
    t2 = Clock::now();
    double d_fast = std::chrono::duration<double>(t2 - t1).count();
    double mbps_fast = ((double)vectors * N / 1e6) / d_fast;

    std::cout << "| " << std::setw(2) << m << " | " << std::setw(9) << N
              << " | " << std::setw(2) << t << " | " << std::setw(10)
              << std::fixed << std::setprecision(1) << mbps_std << " | "
              << std::setw(11) << std::fixed << std::setprecision(1)
              << mbps_fast << " | " << std::setw(6) << std::fixed
              << std::setprecision(1) << (mbps_fast / mbps_std) << "x |\n";
  }
  return 0;
}
