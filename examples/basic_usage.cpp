#include <iostream>
#include <litebch/LiteBCH.h>
#include <vector>

void print_bits(const std::string &label, const std::vector<int> &bits) {
  std::cout << label << ": ";
  for (int b : bits)
    std::cout << b;
  std::cout << "\n";
}

int main() {
  // 1. Configure BCH(N=31, t=3) -> m=5
  int N = 31;
  int t = 3;

  std::cout << "LiteBCH Example (N=" << N << ", t=" << t << ")\n";
  lite::LiteBCH bch(N, t);

  // 2. Prepare Data (K bits)
  int K = bch.get_K(); // Should be 16
  std::vector<int> message(K, 0);
  // Fill with pattern 1010...
  for (int i = 0; i < K; ++i)
    message[i] = (i % 2);

  print_bits("Original", message);

  // 3. Encode
  std::vector<int> encoded = bch.encode(message);
  print_bits("Encoded ", encoded);

  // 4. Corrupt (Add 2 errors)
  encoded[5] ^= 1;
  encoded[10] ^= 1;
  print_bits("Corrupt ", encoded);

  // 5. Decode
  std::vector<int> decoded;
  if (bch.decode(encoded, decoded)) {
    std::cout << "Decode: SUCCESS\n";
    print_bits("Decoded ", decoded);
  } else {
    std::cout << "Decode: FAILED\n";
  }

  // Verify
  if (decoded == message) {
    std::cout << "Result: PERFECT MATCH\n";
  } else {
    std::cout << "Result: MISMATCH\n";
  }

  return 0;
}
