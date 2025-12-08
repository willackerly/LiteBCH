#include <algorithm>
#include <cmath>
#include <iostream>
#include <litebch/LiteBCH.h>
#include <numeric>

namespace lite {

LiteBCH::LiteBCH(int N, int t, std::vector<I> p) : N(N), t(t), d(2 * t + 1) {
  m = (int)std::ceil(std::log2(N));
  if (N != ((1 << m) - 1)) {
    throw std::invalid_argument("N must be 2^m - 1");
  }

  // 1. Initialize Galois Field
  alpha_to.resize(N + 1);
  index_of.resize(N + 1);

  if (!p.empty()) {
    if ((int)p.size() != m + 1) {
      throw std::invalid_argument(
          "Primitive polynomial p must have size m + 1");
    }
    this->p = p;
  } else {
    this->p.resize(m + 1, 0); // Primitive polynomial coeffs
    select_polynomial();
  }

  init_galois();

  // 2. Compute Generator Polynomial
  compute_generator_polynomial();

  // 3. Set Dimensions
  n_rdncy = g.size() - 1;
  K = N - n_rdncy;

  // 4. Init Fast Encoding Tables
  ecc_bits = N - K;
  ecc_words = (ecc_bits + 31) / 32;
  ecc_bytes = (ecc_bits + 7) / 8;
  init_fast_tables();

  // 5. Init Decoder Buffers
  // Optimized for scalability: elp only needs 2*t history
  int t2 = 2 * t;
  elp.resize(t2 + 5, std::vector<int>(N + 1)); // 2nd dim could be smaller but N
                                               // is safe upper bound for sparse
  discrepancy.resize(t2 + 5);
  l.resize(t2 + 5);
  u_lu.resize(t2 + 5);
  s.resize(2 * t + 1);
  loc.resize(t + 1);
  reg.resize(t + 1);

  reg.resize(t + 1);
}

// ==========================================
// Galois Field Logic (Masked form aff3ct/Tools/Math/Galois)
// ==========================================

void LiteBCH::select_polynomial() {
  p[0] = p[m] = 1;
  if (m == 3)
    p[1] = 1;
  else if (m == 4)
    p[1] = 1;
  else if (m == 5)
    p[2] = 1;
  else if (m == 6)
    p[1] = 1;
  else if (m == 7)
    p[1] = 1;
  else if (m == 8)
    p[4] = p[5] = p[6] = 1;
  else if (m == 9)
    p[4] = 1;
  else if (m == 10)
    p[3] = 1;
  else if (m == 11)
    p[2] = 1;
  else if (m == 12)
    p[3] = p[4] = p[7] = 1;
  else if (m == 13)
    p[1] = p[3] = p[4] = 1;
  else if (m == 14)
    p[1] = p[11] = p[12] = 1;
  else if (m == 15)
    p[1] = 1;
  else if (m == 16)
    p[2] = p[3] = p[5] = 1;
  // Add more if needed, supports up to m=16 for typical BCH use
}

void LiteBCH::init_galois() {
  int i, mask;
  mask = 1;
  alpha_to[m] = 0;
  for (i = 0; i < m; i++) {
    alpha_to[i] = mask;
    index_of[alpha_to[i]] = i;
    if (p[i] != 0)
      alpha_to[m] ^= mask;
    mask <<= 1;
  }
  index_of[alpha_to[m]] = m;
  mask >>= 1;
  for (i = m + 1; i < N; i++) {
    if (alpha_to[i - 1] >= mask)
      alpha_to[i] = alpha_to[m] ^ ((alpha_to[i - 1] ^ mask) << 1);
    else
      alpha_to[i] = alpha_to[i - 1] << 1;
    const auto idx = alpha_to[i];
    index_of[idx] = i;
  }
  index_of[0] = -1;
}

// ==========================================
// Generator Poly Logic (from BCH_polynomial_generator)
// ==========================================

void LiteBCH::compute_generator_polynomial() {
  std::vector<std::vector<int>> cycle_sets(2, std::vector<int>(1));
  cycle_sets[0][0] = 0;
  cycle_sets[1][0] = 1;
  int cycle_set_representative = 0;
  bool test;
  do {
    auto &cycle_set = cycle_sets.back();
    cycle_set.reserve(32);
    do {
      cycle_set.push_back((cycle_set.back() * 2) % N);
    } while (((cycle_set.back() * 2) % N) != cycle_set.front());

    do {
      cycle_set_representative++;
      test = false;
      for (unsigned i = 1; (i < cycle_sets.size()) && !test; i++)
        for (unsigned j = 0; (j < cycle_sets[i].size()) && !test; j++)
          test = (cycle_set_representative == cycle_sets[i][j]);
    } while (test && (cycle_set_representative < (N - 1)));

    if (!test) {
      cycle_sets.push_back(std::vector<int>(1, cycle_set_representative));
    }
  } while (cycle_set_representative < (N - 1));

  int rdncy = 0;
  std::vector<int> min;
  for (unsigned i = 1; i < cycle_sets.size(); i++) {
    test = false;
    for (unsigned j = 0; (j < cycle_sets[i].size()) && !test; j++)
      for (int root = 1; (root < d) && !test; root++)
        if (root == cycle_sets[i][j]) {
          test = true;
          min.push_back((int)i);
          rdncy += (int)cycle_sets[i].size();
        }
  }

  std::vector<int> zeros(rdncy + 1);
  int kaux = 1;
  test = true;
  for (unsigned i = 0; i < min.size() && test; i++)
    for (unsigned j = 0; j < cycle_sets[min[i]].size() && test; j++) {
      zeros[kaux] = cycle_sets[min[i]][j];
      kaux++;
      test = (kaux <= rdncy);
    }

  g.resize(rdncy + 1);
  g[0] = alpha_to[zeros[1]];
  g[1] = 1;
  for (int i = 2; i <= rdncy; i++) {
    g[i] = 1;
    for (int j = i - 1; j > 0; j--)
      if (g[j] != 0)
        g[j] = g[j - 1] ^ alpha_to[(index_of[g[j]] + zeros[i]) % N];
      else
        g[j] = g[j - 1];
    g[0] = alpha_to[(index_of[g[0]] + zeros[i]) % N];
  }

  // Force binary coefficients (GF(2))
  for (auto &val : g)
    val &= 1;
}

// ==========================================
// Encoding Logic (from Encoder_BCH)
// ==========================================

// --- Fast Encoding Helpers ---

static uint8_t get_top_byte(const std::vector<uint32_t> &par, int n_bits) {
  uint8_t res = 0;
  for (int i = 0; i < 8; ++i) {
    int bit_pos = n_bits - 8 + i;
    if (bit_pos < 0)
      continue;
    int w = bit_pos / 32;
    int b = bit_pos % 32;
    if (par[w] & (1 << b)) {
      res |= (1 << i);
    }
  }
  return res;
}

static void shift_left_8(std::vector<uint32_t> &par, int words) {
  uint32_t carry = 0;
  for (int w = 0; w < words; ++w) {
    uint32_t temp = par[w];
    par[w] = (temp << 8) | carry;
    carry = (temp >> 24);
  }
}

void LiteBCH::init_fast_tables() {
  encode_lut.resize(256, std::vector<uint32_t>(ecc_words, 0));

  // Simulate 8-bit LFSR for every byte input
  for (int i = 0; i < 256; ++i) {
    // Temp remainder
    std::vector<int> rem(ecc_bits, 0);

    // Shift in byte 'i' (MSB first logic)
    for (int bit = 7; bit >= 0; --bit) {
      int input = (i >> bit) & 1;
      int feedback = input ^ rem[ecc_bits - 1]; // Fix: Include state feedback!

      // Shift and add g
      for (int k = ecc_bits - 1; k > 0; --k) {
        // Legacy __encode uses (g[j] & feedback) for j > 0
        rem[k] = rem[k - 1] ^ (g[k] & feedback);
      }
      // Legacy uses (g[0] && feedback) for j = 0
      rem[0] = (g[0] ? feedback : 0);
    }

    // Pack into 32-bit words (Little-Endian: rem[0] -> bit 0)
    for (int w = 0; w < ecc_words; ++w) {
      uint32_t word = 0;
      for (int b = 0; b < 32; ++b) {
        int idx = w * 32 + b;
        if (idx < ecc_bits) {
          if (rem[idx])
            word |= (1 << b);
        }
      }
      encode_lut[i][w] = word;
    }
  }

  // --- Initialize Syndrome LUT for Fast Decoding ---
  // syndrome_lut[i][b] = sum( bit_p * alpha^(i*p) ) for p=0..7
  syndrome_lut.resize(2 * t + 1, std::vector<int>(256, 0));
  for (int i = 1; i <= 2 * t; ++i) {
    for (int b = 0; b < 256; ++b) {
      int val = 0; // Poly form
      for (int p = 0; p < 8; ++p) {
        if ((b >> p) & 1) {
          // Term alpha^(i*p)
          int term = alpha_to[(i * p) % N];
          val ^= term;
        }
      }
      syndrome_lut[i][b] = val;
    }
  }
  // --- Initialize Syndrome LUT for Fast Decoding ---
  // syndrome_lut[i][b] = sum( bit_p * alpha^(i*p) ) for p=0..7
  syndrome_lut.resize(2 * t + 1, std::vector<int>(256, 0));
  for (int i = 1; i <= 2 * t; ++i) {
    for (int b = 0; b < 256; ++b) {
      int val = 0; // Poly form
      for (int p = 0; p < 8; ++p) {
        if ((b >> p) & 1) {
          // Term alpha^(i*p)
          int term = alpha_to[(i * p) % N];
          val ^= term;
        }
      }
      syndrome_lut[i][b] = val;
    }
  }
}

static void apply_mask(std::vector<uint32_t> &par, int n_bits) {
  for (size_t w = 0; w < par.size(); ++w) {
    int start_bit = w * 32;
    if (start_bit >= n_bits) {
      par[w] = 0;
    } else {
      int end_bit = start_bit + 32;
      if (end_bit > n_bits) {
        int valid_bits = n_bits - start_bit;
        if (valid_bits < 32)
          par[w] &= (1U << valid_bits) - 1;
      }
    }
  }
}

// --- Byte-Oriented Encoding (Fast LUT + Bitwise Tail) ---
void LiteBCH::encode(const uint8_t *data, size_t len, uint8_t *ecc_out) {
  // State: 'par' (parity) stored as 32-bit words
  std::vector<uint32_t> par(ecc_words, 0);

  size_t full_bytes = K / 8;
  int rem_bits = K % 8;

  // 1. Fast LUT for full bytes
  for (size_t i = 0; i < full_bytes; ++i) {
    uint8_t input = data[i];
    uint8_t feedback = get_top_byte(par, ecc_bits) ^ input;
    shift_left_8(par, ecc_words);
    apply_mask(
        par,
        ecc_bits); // Masking needed if ecc_bits < 8? Or just consistency.

    const auto &mask = encode_lut[feedback];
    for (int w = 0; w < ecc_words; ++w) {
      par[w] ^= mask[w];
    }
  }

  // 2. Bitwise processing for remaining bits (if any)
  if (rem_bits > 0) {
    uint8_t last_byte = data[full_bytes];
    // Bits are packed MSB first (7..0).
    // We only process rem_bits from the top (7 downto 8-rem_bits).
    for (int b = 0; b < rem_bits; ++b) {
      int bit_pos = 7 - b;
      int input_bit = (last_byte >> bit_pos) & 1;

      // LFSR Step (1 bit)
      // Extract top BIT of par
      // get_top_byte extracts 8 bits. We need bit at ecc_bits-1.
      int top_bit = 0;
      int word_idx = (ecc_bits - 1) / 32;
      int bit_idx = (ecc_bits - 1) % 32;
      if (par[word_idx] & (1U << bit_idx))
        top_bit = 1;

      int feedback = input_bit ^ top_bit;

      // Shift Left 1
      uint32_t carry = 0;
      for (int w = 0; w < ecc_words; ++w) {
        uint32_t temp = par[w];
        par[w] = (temp << 1) | carry;
        carry = (temp >> 31);
      }

      // Add g if feedback
      // g is vector<I>. We need packed g?
      // We don't have packed g. We have g indices.
      // Re-use init_fast_tables logic or pre-compute packed g?
      // Pre-computing packed g is safest. Or just iterate g.
      if (feedback) {
        for (int k = 0; k < ecc_bits; ++k) {
          // g[k] corresponds to x^k.
          // par is packed little endian.
          if (g[k]) { // g is binary now
            int w = k / 32;
            int b = k % 32;
            par[w] ^= (1U << b);
          }
        }
      }
    }
    // Final mask
    apply_mask(par, ecc_bits);
  }

  // Output result
  std::memset(ecc_out, 0, ecc_bytes);
  for (int i = 0; i < ecc_bits; ++i) {
    int w = i / 32;
    int b = i % 32;
    if (par[w] & (1 << b)) {
      ecc_out[i / 8] |= (1 << (i % 8));
    }
  }
}

std::vector<LiteBCH::B> LiteBCH::encode(const std::vector<B> &message_bits) {
  if (message_bits.size() != (size_t)K) {
    throw std::invalid_argument("Message size must be K=" + std::to_string(K));
  }
  std::vector<B> encoded(N);
  // par (parity) will be at the BEGINNING in this logic, then copied?
  // aff3ct typically constructs [Parity | Message] or [Message | Parity]
  // depending on config. Let's decode Encoder_BCH.cpp logic: _encode copies
  // U_K (message) to the END of X_N (codeword) and parity to the beginning.
  // Wait, lines 64-67:
  // __encode(U_K, X_N) -> writes parity to X_N[0..n_rdncy]
  // copy U_K to X_N + n_rdncy.
  // So systematic format is: [Parity | Message] in the output buffer?
  // But usually systematic is [Message | Parity].
  // Let's check __encode: checks "feedback = U_K[i] ^ par[n_rdncy-1]"

  // We will stick to the aff3ct layout: [Parity (n_rdncy) | Message (K)]

  std::vector<B> par(n_rdncy);
  __encode(message_bits.data(), par.data());

  // Copy parity to start
  for (int i = 0; i < n_rdncy; ++i)
    encoded[i] = par[i];
  // Copy message to end
  for (int i = 0; i < K; ++i)
    encoded[n_rdncy + i] = message_bits[i];

  return encoded;
}

void LiteBCH::__encode(const B *U_K, B *par) {
  std::fill(par, par + n_rdncy, (B)0);
  for (auto i = K - 1; i >= 0; i--) {
    const auto feedback = U_K[i] ^ par[n_rdncy - 1];
    for (auto j = n_rdncy - 1; j > 0; j--)
      par[j] = par[j - 1] ^ (g[j] & feedback);
    par[0] =
        feedback ? (g[0] && feedback) : 0; // Fixed boolean logic from original
    // Original: g[0] && feedback. g is int, feedback is bool/int.
    // In simple GF(2), g[j] is 0 or 1.
  }
}

// ==========================================
// Decoding Logic (from Decoder_BCH_std)
// ==========================================

bool LiteBCH::decode(const std::vector<B> &received_bits,
                     std::vector<B> &decoded_message) {
  if (received_bits.size() != (size_t)N)
    return false;

  // Pack Bits to Bytes for Fast Decode
  // Layout: [Parity (n_rdncy) | Message (K)]
  int n_data_bytes = (K + 7) / 8;
  int n_ecc_bytes = ecc_bytes;
  std::vector<uint8_t> data(n_data_bytes, 0);
  std::vector<uint8_t> ecc(n_ecc_bytes, 0);

  // Pack Parity -> ECC (LSB packed)
  for (int i = 0; i < n_rdncy; ++i) {
    if (received_bits[i])
      ecc[i / 8] |= (1 << (i % 8));
  }

  // Pack Message -> Data (MSB packed, High Degree First)
  // Message indices 0..K-1. Index K-1 is High Degree.
  // We want High Degree at Stream Pos 0.
  for (int i = 0; i < K; ++i) {
    if (received_bits[n_rdncy + i]) {
      int stream_pos = K - 1 - i;
      data[stream_pos / 8] |= (1 << (7 - (stream_pos % 8)));
    }
  }

  // Fast Decode
  int count = decode(data.data(), n_data_bytes, ecc.data());

  if (count < 0)
    return false;

  // Unpack Corrected Data
  decoded_message.resize(K);
  for (int i = 0; i < K; ++i) {
    int stream_pos = K - 1 - i;
    int byte_idx = stream_pos / 8;
    int bit_idx = 7 - (stream_pos % 8);
    decoded_message[i] = (data[byte_idx] >> bit_idx) & 1;
  }

  return true;
}

int LiteBCH::_decode(B *Y_N) {
  int i, j, syn_error = 0;
  int t2 = 2 * t;
  int N_p2_1 = N; // Assuming N is 2^m - 1

  /* first form the syndromes */
  for (i = 1; i <= t2; i++) {
    s[i] = 0;
    for (j = 0; j < N; j++)
      if (Y_N[j] != 0)
        s[i] ^= alpha_to[(i * j) % N_p2_1];
    if (s[i] != 0)
      syn_error = 1;
    s[i] = (int)index_of[s[i]];
  }

  if (!syn_error)
    return 0; // Success

  /* Berlekamp iterative algorithm */
  discrepancy[0] = 0;
  discrepancy[1] = s[1];
  elp[0][0] = 0;
  elp[1][0] = 1;
  for (i = 1; i < t2; i++) {
    elp[0][i] = -1;
    elp[1][i] = 0;
  }
  l[0] = 0;
  l[1] = 0;
  u_lu[0] = -1;
  u_lu[1] = 0;

  int q, u = 0;
  do {
    u++;
    if (discrepancy[u] == -1) {
      l[u + 1] = l[u];
      for (i = 0; i <= l[u]; i++) {
        elp[u + 1][i] = elp[u][i];
        elp[u][i] = (int)index_of[elp[u][i]];
      }
    } else {
      q = u - 1;
      while ((discrepancy[q] == -1) && (q > 0))
        q--;
      if (q > 0) {
        j = q;
        do {
          j--;
          if ((discrepancy[j] != -1) && (u_lu[q] < u_lu[j]))
            q = j;
        } while (j > 0);
      }

      if (l[u] > l[q] + u - q)
        l[u + 1] = l[u];
      else
        l[u + 1] = l[q] + u - q;

      for (i = 0; i < t2; i++)
        elp[u + 1][i] = 0;
      for (i = 0; i <= l[q]; i++)
        if (elp[q][i] != -1)
          elp[u + 1][i + u - q] = (int)
              alpha_to[(discrepancy[u] + N_p2_1 - discrepancy[q] + elp[q][i]) %
                       N_p2_1];
      for (i = 0; i <= l[u]; i++) {
        elp[u + 1][i] ^= elp[u][i];
        elp[u][i] = (int)index_of[elp[u][i]];
      }
    }
    u_lu[u + 1] = u - l[u + 1];

    if (u < t2) {
      if (s[u + 1] != -1)
        discrepancy[u + 1] = (int)alpha_to[s[u + 1]];
      else
        discrepancy[u + 1] = 0;

      for (i = 1; i <= l[u + 1]; i++)
        if ((s[u + 1 - i] != -1) && (elp[u + 1][i] != 0))
          discrepancy[u + 1] ^=
              alpha_to[(s[u + 1 - i] + index_of[elp[u + 1][i]]) % N_p2_1];
      discrepancy[u + 1] = (int)index_of[discrepancy[u + 1]];
    }
  } while ((u < t2) && (l[u + 1] <= t));

  u++;
  if (l[u] <= t) {
    for (i = 0; i <= l[u]; i++)
      elp[u][i] = (int)index_of[elp[u][i]];

    // Chien search
    for (i = 1; i <= l[u]; i++)
      reg[i] = elp[u][i];
    int count = 0;
    for (i = 1; i <= N_p2_1; i++) {
      q = 1;
      for (j = 1; j <= l[u]; j++)
        if (reg[j] != -1) {
          reg[j] = (reg[j] + j) % N_p2_1;
          q ^= alpha_to[reg[j]];
        }
      if (!q) {
        loc[count++] = N_p2_1 - i;
      }
    }

    if (count == l[u]) {
      for (i = 0; i < l[u]; i++)
        if (loc[i] < N)
          Y_N[loc[i]] ^= 1;
      return 0; // Success
    } else {
      return 1; // Failure
    }
  }
  return 1; // Failure
}

// ==========================================
// Fast Byte-Oriented Decoding
// ==========================================
int LiteBCH::decode(uint8_t *data, size_t len, uint8_t *ecc) {
  // 1. Syndrome Calculation via Re-Encoding
  // Optimization: S_j = R(alpha^j) = (Data*x^r + Ecc_recv)(alpha^j)
  // We know Data*x^r = Q*g + Ecc_calc.
  // So Data*x^r(alpha^j) = Ecc_calc(alpha^j).
  // Thus S_j = Ecc_calc(alpha^j) + Ecc_recv(alpha^j) = (Ecc_calc +
  // Ecc_recv)(alpha^j). We only need to compute syndromes on the XOR
  // difference of ECCs.

  std::vector<int> s_poly(2 * t + 1, 0); // Syndromes

  // Compute Calc ECC
  std::vector<uint8_t> calc_ecc(ecc_bytes, 0);
  encode(data, len, calc_ecc.data());

  // XOR with Recv ECC to get difference polynomial
  for (int i = 0; i < ecc_bytes; ++i) {
    calc_ecc[i] ^= ecc[i];
  }

  // Precompute alpha^(8*i) exponents for fast Horner multiplication
  // alpha_8_pow[i] stores 'k' such that alpha^k = (alpha^i)^8
  std::vector<int> alpha_8_pow(2 * t + 1);
  for (int i = 1; i <= 2 * t; ++i) {
    alpha_8_pow[i] = (i * 8) % N;
  }

  // Process ECC Diff Bytes (Low Degree First? Check Encoder.)
  // Encoder puts Parity at Low Degrees?
  // Our encode() produces ECC bytes.
  // init_fast_tables packs: rem[0] -> bit 0 of word 0.
  // The LFSR state 'rem' represents coeff of x^0 ... x^r-1.
  // So ecc[0] contains x^0..x^7.
  // Thus we process ECC bytes from High Index (High Degree) to Low Index (Low
  // Degree) for Horner. Wait, ECC[0] is x^0...x^7 (Low Degree). Horner
  // scheme: ( ... ((C_k * x + C_{k-1}) * x + ... ) We want to evaluate at
  // alpha^j. Poly = P_0 + P_1 x + ... = P_0 + x(P_1 + x(...)) Byte wise: B_0
  // + B_1 x^8 + ... = B_0 + x^8(B_1 + x^8(B_2 ...)) No, that's expanding from
  // 0. Horner usually starts from High Degree. ECC High bytes are at END of
  // vector (if Little Endian polynomial). ecc[0] is Low Degree. ecc[last] is
  // High Degree. So iterate k from ecc_len-1 down to 0.

  int ecc_len = ecc_bytes;
  for (int k = ecc_len - 1; k >= 0; --k) {
    uint8_t b = calc_ecc[k];
    if (k == ecc_len - 1) {
      int valid_ecc = n_rdncy % 8;
      if (valid_ecc != 0)
        b &= ((1 << valid_ecc) - 1);
    }

    for (int i = 1; i <= 2 * t; ++i) {
      if (s_poly[i] != 0) {
        int idx = index_of[s_poly[i]];
        idx = (idx + alpha_8_pow[i]) % N;
        s_poly[i] = alpha_to[idx];
      }
      s_poly[i] ^= syndrome_lut[i][b];
    }
  }

  // No combining needed! s_poly holds the full syndromes.
  bool syn_error = false;
  for (int i = 1; i <= 2 * t; ++i) {
    if (s_poly[i] != 0)
      syn_error = true;
  }

  // Convert S to Index Form for Berlekamp
  for (int i = 1; i <= 2 * t; ++i) {
    if (s_poly[i] != 0) {
      s_poly[i] = index_of[s_poly[i]];
      syn_error = true;
    } else {
      s_poly[i] = -1; // -1 for Zero element in index form
    }
  }

  if (!syn_error)
    return 0;

  // 2. Berlekamp-Massey (Copied logic)
  this->s = s_poly;

  discrepancy[0] = 0;
  discrepancy[1] = s[1];
  elp[0][0] = 0;
  elp[1][0] = 1;
  int t2 = 2 * t;
  for (int i = 1; i < t2; i++) {
    elp[0][i] = -1;
    elp[1][i] = 0;
  }
  l[0] = 0;
  l[1] = 0;
  u_lu[0] = -1;
  u_lu[1] = 0;

  int q, u = 0, j;
  int N_p2_1 = N;

  do {
    u++;
    if (discrepancy[u] == -1) {
      l[u + 1] = l[u];
      for (int i = 0; i <= l[u]; i++) {
        elp[u + 1][i] = elp[u][i];
        elp[u][i] = (int)index_of[elp[u][i]];
      }
    } else {
      q = u - 1;
      while ((discrepancy[q] == -1) && (q > 0))
        q--;
      if (q > 0) {
        j = q;
        do {
          j--;
          if ((discrepancy[j] != -1) && (u_lu[q] < u_lu[j]))
            q = j;
        } while (j > 0);
      }

      if (l[u] > l[q] + u - q)
        l[u + 1] = l[u];
      else
        l[u + 1] = l[q] + u - q;

      for (int i = 0; i < t2; i++)
        elp[u + 1][i] = 0;
      for (int i = 0; i <= l[q]; i++)
        if (elp[q][i] != -1)
          elp[u + 1][i + u - q] = (int)
              alpha_to[(discrepancy[u] + N_p2_1 - discrepancy[q] + elp[q][i]) %
                       N_p2_1];
      for (int i = 0; i <= l[u]; i++) {
        elp[u + 1][i] ^= elp[u][i];
        elp[u][i] = (int)index_of[elp[u][i]];
      }
    }
    u_lu[u + 1] = u - l[u + 1];

    if (u < t2) {
      if (s[u + 1] != -1)
        discrepancy[u + 1] = (int)alpha_to[s[u + 1]];
      else
        discrepancy[u + 1] = 0;

      for (int i = 1; i <= l[u + 1]; i++)
        if ((s[u + 1 - i] != -1) && (elp[u + 1][i] != 0))
          discrepancy[u + 1] ^=
              alpha_to[(s[u + 1 - i] + index_of[elp[u + 1][i]]) % N_p2_1];
      discrepancy[u + 1] = (int)index_of[discrepancy[u + 1]];
    }
  } while ((u < t2) && (l[u + 1] <= t));

  u++;
  if (l[u] <= t) {
    for (int i = 0; i <= l[u]; i++)
      elp[u][i] = (int)index_of[elp[u][i]];

    for (int i = 1; i <= l[u]; i++)
      reg[i] = elp[u][i];
    int count = 0;
    // Chien Search Optimization: Remove modulo
    for (int i = 1; i <= N_p2_1; i++) {
      q = 1;
      for (int j = 1; j <= l[u]; j++)
        if (reg[j] != -1) {
          int val = reg[j] + j;
          if (val >= N_p2_1)
            val -= N_p2_1;
          reg[j] = val;
          q ^= alpha_to[val];
        }
      if (!q) {
        loc[count++] = N_p2_1 - i;
      }
    }

    if (count == l[u]) {
      for (int i = 0; i < l[u]; i++) {
        int bit_idx = loc[i];
        if (bit_idx >= n_rdncy) {
          int d_idx = bit_idx - n_rdncy;
          // Data is packed High Degree First.
          // d_idx is Degree (Low->High).
          // Map Degree to Stream Position.
          int stream_pos = K - 1 - d_idx;
          int byte_idx = stream_pos / 8;
          int bit_off = 7 - (stream_pos % 8);
          if (byte_idx < (int)len) {
            data[byte_idx] ^= (1 << bit_off);
          }
        } else {
          int byte_idx = bit_idx / 8;
          int bit_off = bit_idx % 8;
          if (byte_idx < ecc_bytes) {
            ecc[byte_idx] ^= (1 << bit_off);
          }
        }
      }
      return count;
    } else {
      return -1;
    }
  }
  return -1;
}

std::vector<LiteBCH::B> string_to_bits(const std::string &str) {
  std::vector<LiteBCH::B> bits(str.length() * 8);
  for (size_t i = 0; i < str.length(); ++i) {
    unsigned char c = str[i];
    for (int j = 0; j < 8; ++j) {
      bits[i * 8 + j] = (c >> j) & 1; // LSB first
    }
  }
  return bits;
}

std::string bits_to_string(const std::vector<LiteBCH::B> &bits) {
  std::string str(bits.size() / 8, ' ');
  for (size_t i = 0; i < str.length(); ++i) {
    unsigned char c = 0;
    for (int j = 0; j < 8; ++j) {
      if (bits[i * 8 + j])
        c |= (1 << j);
    }
    str[i] = c;
  }
  return str;
}

} // namespace lite
