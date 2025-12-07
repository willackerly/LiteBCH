#include <iostream>
#include <litebch/LiteBCH.h>
#include <string>
#include <vector>

// Simple assertion helper
#define ASSERT_TRUE(cond, msg)                                                 \
  if (!(cond)) {                                                               \
    std::cout << "FAIL: " << msg << std::endl;                                 \
    return 1;                                                                  \
  }
#define ASSERT_EQ(a, b, msg)                                                   \
  if ((a) != (b)) {                                                            \
    std::cout << "FAIL: " << msg << " Expected " << (a) << " got " << (b)      \
              << std::endl;                                                    \
    return 1;                                                                  \
  }
#define PASS(msg) std::cout << "PASS: " << msg << std::endl;

int main() {
  std::cout << "Starting LiteBCH Tests..." << std::endl;

  // Configuration
  int m = 5; // N=31
  int t = 3;
  int N = (1 << m) - 1;

  std::cout << "Initializing BCH(N=" << N << ", t=" << t << ")..." << std::endl;
  lite::LiteBCH bch(N, t);

  // 1. Text to bits
  int K = bch.get_K();
  std::cout << "K=" << K << ", Redundancy=" << (N - K) << std::endl;

  // Create a message of exactly K bits
  std::vector<int> message(K);
  for (int i = 0; i < K; ++i)
    message[i] = (i % 2); // 010101 pattern

  // 2. Encode
  std::cout << "Encoding..." << std::endl;
  std::vector<int> encoded = bch.encode(message);
  std::cout << "Encoded Bits: ";
  for (int b : encoded)
    std::cout << b;
  std::cout << "\n";
  ASSERT_EQ((int)encoded.size(), N, "Encoded size mismatch");

  // 3. Decode Clean
  std::cout << "Decoding (Clean)..." << std::endl;
  std::vector<int> decoded;
  bool success = bch.decode(encoded, decoded);
  if (!success) {
    std::cout << "FAIL: Clean decode reported failure" << std::endl;
    return 1;
  }

  if (decoded != message) {
    std::cout << "FAIL: Clean decode content mismatch" << std::endl;
    return 1;
  }
  PASS("Clean decode");

  // 4. Decode with 1 error
  std::cout << "Testing 1-bit error correction..." << std::endl;
  std::vector<int> corrupted = encoded;
  corrupted[0] ^= 1; // Flip first bit

  std::vector<int> decoded_err;
  success = bch.decode(corrupted, decoded_err);
  ASSERT_TRUE(success, "Failed to correct 1 error");

  if (decoded_err != message) {
    std::cout << "FAIL: 1-bit error correction content mismatch" << std::endl;
    return 1;
  }
  PASS("1-bit error correction");

  std::cout << "ALL TESTS PASSED." << std::endl;
  return 0;
}
