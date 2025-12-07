#include <iostream>
#include <litebch/LiteBCH.h>
#include <string>
#include <vector>

int main() {
  int N = 1023;

  std::vector<int> t_values = {1, 2, 5, 10, 20, 30, 40, 45, 50};

  for (int t : t_values) {
    std::cout << "\nTesting t=" << t << "..." << std::endl;

    // Explicit poly from legacy_compat_test for m=10
    std::vector<int> p = {1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1};
    lite::LiteBCH bch(N, t, p);
    int K = bch.get_K();
    int real_t = bch.get_t();
    std::cout << "  Requested t=" << t << ", Got t=" << real_t << ", K=" << K
              << std::endl;
    std::cout << "  Redundancy=" << (N - K) << " bits ("
              << (double)(N - K) / 10.0 << " symbols)" << std::endl;

    std::vector<int> message(K);
    for (int i = 0; i < K; ++i)
      message[i] = (i % 2);

    auto encoded = bch.encode(message);

    // Inject 5 errors
    std::vector<int> corrupted = encoded;
    int err_count = 5;
    if (real_t < 5) {
      std::cout << "  Skipping (t=" << real_t << " < 5 errors)" << std::endl;
      continue;
    }
    for (int i = 0; i < err_count; ++i) {
      corrupted[i * 10] ^= 1;
    }

    std::vector<int> decoded;
    bool success = bch.decode(corrupted, decoded);

    if (success) {
      std::cout << "  [INFO] Decode returned true (Success).\n";
    } else {
      std::cout << "  [WARN] Decode returned false (Failure).\n";
    }

    if (decoded == message) {
      std::cout << "  [PASS] Content Matches Expected.\n";
    } else {
      std::cout << "  [FAIL] Content Mismatch!\n";
      // Print first few mismatches?
    }
  }

  return 0;
}
