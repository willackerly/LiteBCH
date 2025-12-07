#include <iomanip>
#include <iostream>
#include <litebch/LiteBCH.h>
#include <random>
#include <vector>

// Helper to print bits
void print_bits(const std::vector<int> &bits, int limit = 20) {
  for (size_t i = 0; i < bits.size() && i < (size_t)limit; ++i) {
    std::cout << bits[i];
  }
  if (bits.size() > (size_t)limit)
    std::cout << "...";
  std::cout << "\n";
}

// Accessor subclass
class TestBCH : public lite::LiteBCH {
public:
  using lite::LiteBCH::LiteBCH;

  bool make_sure_poly_is_binary() {
    bool binary = true;
    std::cout << "  Generator Polynomial (deg " << g.size() - 1 << "): ";
    for (size_t i = 0; i < g.size(); ++i) {
      if (i < 10)
        std::cout << g[i] << " ";
      else if (i == 10)
        std::cout << "... ";

      if (g[i] != 0 && g[i] != 1) {
        binary = false;
      }
    }
    std::cout << "\n";
    if (!binary) {
      std::cout << "  [FAIL] Generator Poly has non-binary coefficients!\n";
      // Print first offender
      for (size_t i = 0; i < g.size(); ++i) {
        if (g[i] != 0 && g[i] != 1) {
          std::cout << "         g[" << i << "] = " << g[i] << "\n";
          break;
        }
      }
    } else {
      std::cout << "  [OK] Generator Poly is binary.\n";
    }
    return binary;
  }

  void print_gf_info() {
    std::cout << "  GF(2^" << m << ") First 10 alpha_to: ";
    for (int i = 0; i < 10; ++i)
      std::cout << alpha_to[i] << " ";
    std::cout << "\n";
  }
};

// Simple Linear Congruential Generator for deterministic testing (Same as
// repro_common.h)
class SimpleLCG {
  uint32_t state;

public:
  SimpleLCG(uint32_t seed) : state(seed) {}
  uint32_t next() {
    state = state * 1664525 + 1013904223;
    return state;
  }
  // Generate 0 or 1
  int next_bit() { return (next() >> 31) & 1; }
};

// Checksum helper (DJB2-like)
uint32_t calculate_checksum(const std::vector<int> &data) {
  uint32_t hash = 5381;
  for (int bit : data) {
    hash = ((hash << 5) + hash) + bit;
  }
  return hash;
}

bool test_config(int m, int t) {
  int N = (1 << m) - 1;
  std::cout << "\n========================================\n";
  std::cout << "Testing Config: m=" << m << ", N=" << N << ", t=" << t << "\n";
  std::cout << "========================================\n";

  TestBCH bch(N, t);
  int K = bch.get_K();
  int real_t = bch.get_t();
  std::cout << "  K=" << K << ", Redundancy=" << (N - K)
            << ", Real t=" << real_t << "\n";

  bch.print_gf_info();
  if (!bch.make_sure_poly_is_binary())
    return false; // Fail early if poly broken

  // 1. Bit-Wise API Test (Legacy)
  std::cout << "\n[Test 1] Bit-Wise API (Legacy)\n";
  std::vector<int> msg_bits(K);
  for (int i = 0; i < K; ++i)
    msg_bits[i] = (i % 2); // 010101...

  // Encode
  auto codeword = bch.encode(msg_bits);
  std::cout << "  Encoded size: " << codeword.size() << " (Expected " << N
            << ")\n";

  // Corrupt
  auto corrupted = codeword;
  int errors_injected = std::min(t, 5); // Inject up to 5 or t
  std::cout << "  Injecting " << errors_injected << " errors...\n";
  for (int i = 0; i < errors_injected; ++i)
    corrupted[i * 2] ^= 1;

  // Decode
  std::vector<int> decoded_bits;
  bool success = bch.decode(corrupted, decoded_bits);

  bool pass_legacy = false;
  if (success && decoded_bits == msg_bits) {
    std::cout << "  [PASS] Bit-Wise Decode Success\n";
    pass_legacy = true;
  } else {
    std::cout << "  [FAIL] Bit-Wise Decode Failed (Success=" << success
              << ")\n";
  }

  // 2. Byte-Wise API Test (Fast)
  std::cout << "\n[Test 2] Byte-Wise API (Fast)\n";

  // Prepare byte buffers using SAME message bits (for consistency)
  int data_bytes_len = (K + 7) / 8;
  std::vector<uint8_t> data_bytes(data_bytes_len, 0);

  // PACKING: MSB First (matches Legacy generator order)
  // message[K-1] at data[0] bit 7.
  for (int i = 0; i < K; ++i) {
    if (msg_bits[i]) {
      int pos_from_start = (K - 1 - i);
      int byte_idx = pos_from_start / 8;
      int bit_in_byte = 7 - (pos_from_start % 8);
      data_bytes[byte_idx] |= (1 << bit_in_byte);
    }
  }

  int ecc_bytes_len = (bch.get_N() - bch.get_K() + 7) / 8;
  std::vector<uint8_t> ecc_out(ecc_bytes_len);

  // Encode
  bch.encode(data_bytes.data(), data_bytes_len, ecc_out.data());
  std::cout << "  Encoded bytes generated.\n";

  // Reconstruct full bit vector from bytes to use decode() (since decode is
  // only bitwise currently) Wait, LiteBCH currently ONLY has bitwise decode
  // exposed in header? Yes: bool decode(const std::vector<B> &received_bits,
  // ...) So we must manually convert bytes -> bits to test verify.

  std::vector<int> fast_codeword(N, 0);

  // Fill Parity Part (Indices 0 to n_rdncy-1)
  // ecc_out contains par[0]..par[n-1] (LSB packed).
  int n_red = N - K;
  for (int i = 0; i < n_red; ++i) {
    if (ecc_out[i / 8] & (1 << (i % 8)))
      fast_codeword[i] = 1;
  }

  // Fill Message Part (Indices n_rdncy to N-1)
  for (int i = 0; i < K; ++i) {
    if (msg_bits[i])
      fast_codeword[n_red + i] = 1;
  }

  // Check Codeword Match
  if (fast_codeword != codeword) {
    std::cout << "  [ALARM] Generated Codewords DO NOT MATCH!\n";
    // Find first diff
    for (size_t i = 0; i < N; ++i) {
      if (fast_codeword[i] != codeword[i]) {
        std::cout << "          First diff at index " << i
                  << ": Legacy=" << codeword[i] << " Fast=" << fast_codeword[i]
                  << "\n";
        break;
      }
    }
  } else {
    std::cout << "  [OK] Generated Codewords Match Exactly.\n";
  }

  // Calculate Checksums
  uint32_t leg_sum = calculate_checksum(codeword);
  uint32_t fast_sum = calculate_checksum(fast_codeword);
  std::cout << "  [CHECK] Legacy Checksum: " << leg_sum << "\n";
  std::cout << "  [CHECK] Fast   Checksum: " << fast_sum << "\n";

  // Verify against Golden Values (Regression Guard)
  if (m == 5 && t == 3) {
    if (leg_sum != 2064607224) {
      std::cout << "  [FAIL] Checksum Mismatch (Expected 2064607224)\n";
      pass_legacy = false;
    }
  } else if (m == 10 && t == 50) {
    if (leg_sum != 4172005083) {
      std::cout << "  [FAIL] Checksum Mismatch (Expected 4172005083)\n";
      pass_legacy = false;
    }
  } else if (m == 8 && t == 10) {
    if (leg_sum != 511784257) {
      std::cout << "  [FAIL] Checksum Mismatch (Expected 511784257)\n";
      pass_legacy = false;
    }
  }

  // Corrupt
  auto fast_corrupted = fast_codeword;
  for (int i = 0; i < errors_injected; ++i)
    fast_corrupted[i * 2] ^= 1;

  // Decode
  std::vector<int> fast_decoded_bits;
  success = bch.decode(fast_corrupted, fast_decoded_bits);

  // Verify against expected message bits (derived from input bytes)
  bool pass_fast = false;
  if (success) {
    // Check content
    bool match = true;
    if (fast_decoded_bits != msg_bits) {
      match = false;
    }

    if (match) {
      std::cout
          << "  [PASS] Byte-Wise (+ manual bit conversion) Decode Success\n";
      pass_fast = true;
    } else {
      std::cout << "  [FAIL] Byte-Wise Encode -> Decode Content Mismatch\n";
    }
  } else {
    std::cout << "  [FAIL] Byte-Wise Encode -> Decode Failed\n";

    // DEBUG: Print Vectors
    std::cout << "  DEBUG DUMP:\n";
    std::cout << "  Message: ";
    for (int b : msg_bits)
      std::cout << b;
    std::cout << "\n";

    std::cout << "  Legacy Codeword (Parity Part): ";
    for (int i = K; i < N; ++i)
      std::cout << codeword[i];
    std::cout << "\n";

    std::cout << "  Fast Codeword   (Parity Part): ";
    for (int i = K; i < N; ++i)
      std::cout << fast_codeword[i];
    std::cout << "\n";
  }

  return pass_legacy && pass_fast;
}

int main() {
  bool all_pass = true;

  // Known good small case
  if (!test_config(5, 3))
    all_pass = false;

  // The failing large case
  if (!test_config(10, 50))
    all_pass = false;

  // Intermediate case
  if (!test_config(8, 10))
    all_pass = false;

  return all_pass ? 0 : 1;
}
