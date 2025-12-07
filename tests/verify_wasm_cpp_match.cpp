#include "repro_common.h"
#include <iostream>
#include <litebch/LiteBCH.h>
#include <vector>

int main() {
  // 1. Setup
  // N=255 (m=8), t=10
  int m = 8;
  int N = (1 << m) - 1; // 255
  int t = 10;

  lite::LiteBCH bch(N, t);
  int K = bch.get_K(); // Should be 255 - 10*8 = 175 (approx)

  // 2. Generate Data
  SimpleLCG lcg(42); // Seed 42

  // We'll generate 10 vectors
  int vectors = 10;
  uint32_t total_checksum = 0;

  for (int v = 0; v < vectors; ++v) {
    std::vector<int> message(K);
    for (int i = 0; i < K; ++i) {
      message[i] = lcg.next_bit();
    }

    // Encode
    std::vector<int> encoded = bch.encode(message);

    // Checksum
    total_checksum ^= calculate_checksum(encoded);
  }

  std::cout << "LiteBCH WASM Verification Generator\n";
  std::cout << "================================___\n";
  std::cout << "N=" << N << ", t=" << t << ", V=" << vectors << "\n";
  std::cout << "Golden Checksum: " << total_checksum << "\n";

  return 0;
}
