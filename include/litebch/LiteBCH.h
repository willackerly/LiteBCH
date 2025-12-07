#ifndef LITE_BCH_H
#define LITE_BCH_H

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Standalone BCH implementation extracted from aff3ct (MIT License)

namespace lite {

class LiteBCH {
public:
  using I = int; // Integer type for GF arithmetic
  using B = int; // Bit type (0 or 1)

  // Constructor:
  // N: Codeword length (must be 2^m - 1)
  // t: Correction capability (number of errors)
  // p: (Optional) Primitive polynomial coefficients. If empty, uses default.
  LiteBCH(int N, int t, std::vector<I> p = {});

  // Fast Byte-Oriented Encoding (Re-introducing for verification)
  // Input: data bytes (size K/8)
  // Output: ecc bytes (size ecc_bytes)
  void encode(const uint8_t *data, size_t len, uint8_t *ecc_out);

  // Legacy / Bit-Oriented Encoding (slower, for aff3ct compatibility)
  std::vector<B> encode(const std::vector<B> &message_bits);

  // Decoding:
  // Input: received bits (size N, potentially corrupted)
  // Output: decoded message bits (size K)
  // Returns: true if successful/no error, false if uncorrectable error detected
  bool decode(const std::vector<B> &received_bits,
              std::vector<B> &decoded_message);

  int get_K() const { return K; }
  int get_N() const { return N; }
  int get_t() const { return t; }
  int get_ecc_bytes() const { return ecc_bytes; }

private:
  int N;
  int K; // N - redundancy
  int t;
  int ecc_bytes; // Exposed for convenience

protected:
  int m;       // Order of Galois Field (2^m)
  int d;       // Design distance (2*t + 1)
  int n_rdncy; // Number of parity bits

  std::vector<I> alpha_to; // Log table
  std::vector<I> index_of; // Antilog table
  std::vector<I> p;        // Primitive polynomial
  std::vector<I> g;        // Generator polynomial

private:
  // Decoding buffers
  std::vector<std::vector<int>> elp;
  std::vector<int> discrepancy;
  std::vector<int> l;
  std::vector<int> u_lu;
  std::vector<int> s;
  std::vector<int> loc;
  std::vector<int> reg;

private:
  // Initialization helpers (from Galois & BCH_polynomial_generator)
  void init_galois();
  void select_polynomial();
  void compute_generator_polynomial();

  // Core logic
  void __encode(const B *U_K, B *par);
  int _decode(B *Y_N);
};

// Utility to convert string to bits and back
std::vector<LiteBCH::B> string_to_bits(const std::string &str);
std::string bits_to_string(const std::vector<LiteBCH::B> &bits);

} // namespace lite

#endif // LITE_BCH_H
