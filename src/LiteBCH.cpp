#include <algorithm>
#include <cmath>
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

  // 4. Init Decoder Buffers
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
}

// ==========================================
// Encoding Logic (from Encoder_BCH)
// ==========================================

// --- Byte-Oriented Encoding (Serial LFSR) ---
void LiteBCH::encode(const uint8_t *data, size_t len, uint8_t *ecc_out) {
  // Parity bits, same layout as Legacy (index 0..n_rdncy-1)
  // We use int for parity element to match Legacy 'B' type (usually int or
  // int8)
  std::vector<int> par(n_rdncy, 0);

  int bits_processed = 0;
  // Process input bytes
  for (size_t i = 0; i < len; ++i) {
    uint8_t byte = data[i];
    // Process bits 7 down to 0 (MSB first stream)
    for (int bit = 7; bit >= 0; --bit) {
      if (bits_processed >= K)
        break; // Stop after K bits

      int input_bit = (byte >> bit) & 1;

      // LFSR Step (Identical to __encode)
      // feedback = input ^ par[last]
      int feedback = input_bit ^ par[n_rdncy - 1];

      // Shift and add
      for (int j = n_rdncy - 1; j > 0; --j) {
        par[j] = par[j - 1] ^ (g[j] ? feedback : 0);
      }
      par[0] = (g[0] ? feedback : 0); // g[0] is usually 1

      bits_processed++;
    }
    if (bits_processed >= K)
      break;
  }

  // Pack result into ecc_out
  // Legacy layout: par[0] is first bit, par[n-1] is last.
  // We need to pack this into bytes.
  // Assumption: Output packing should match legacy stream order.
  // Legacy: encoded[0] = par[0].
  // If we map stream -> bytes with bit 0 = bit 0 of byte 0?
  // verification_byte uses `bytes_to_bits` which assumes LSB packing?
  // Wait, my `verification_byte` test compares bits.
  // It converts `ecc_out` bytes to bits using `(bytes[i/8] >> (i%8)) & 1`.
  // So `ecc_out[0]` bit 0 should be `par[0]`.

  int n_ecc_bytes = (n_rdncy + 7) / 8;
  std::memset(ecc_out, 0, n_ecc_bytes);
  for (int i = 0; i < n_rdncy; ++i) {
    if (par[i]) {
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
  // depending on config. Let's decode Encoder_BCH.cpp logic: _encode copies U_K
  // (message) to the END of X_N (codeword) and parity to the beginning. Wait,
  // lines 64-67:
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

  std::vector<B> Y_N = received_bits; // Mutable copy
  int status = _decode(Y_N.data());

  decoded_message.resize(K);
  // Extract message part [Parity | Message]
  for (int i = 0; i < K; ++i)
    decoded_message[i] = Y_N[n_rdncy + i];

  return (status == 0); // _decode returns 0 on success
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
