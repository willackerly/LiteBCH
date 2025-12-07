#ifndef AFF3CT_COMPAT_H
#define AFF3CT_COMPAT_H

#include <litebch/LiteBCH.h>
#include <memory>
#include <vector>

// Compatibility Shim for aff3ct -> LiteBCH
// Allows drop-in replacement for basic BCH usage.

namespace aff3ct {
namespace tools {

// Mimics BCH_polynomial_generator
// Responsibilities: Hold N, t, p. Calculate redundancy (implied).
template <typename I = int> class BCH_polynomial_generator {
public:
  BCH_polynomial_generator(int N, int t, std::vector<I> p = {})
      : N(N), t(t), p(p) {
    // We create a temporary LiteBCH just to calculate K/redundancy correctly
    // avoiding duplicate logic.
    lite::LiteBCH temp(N, t, p);
    n_rdncy = N - temp.get_K();
  }

  int get_n_rdncy() const { return n_rdncy; }
  int get_N() const { return N; }
  int get_t() const { return t; }
  const std::vector<I> &get_p() const { return p; }

private:
  int N, t, n_rdncy;
  std::vector<I> p;
};

} // namespace tools

namespace module {

// Mimics Encoder_BCH
template <typename B = int> class Encoder_BCH {
public:
  Encoder_BCH(int K, int N,
              const tools::BCH_polynomial_generator<B> &poly_gen) {
    // Instantiate the real worker
    bch =
        std::make_shared<lite::LiteBCH>(N, poly_gen.get_t(), poly_gen.get_p());
  }

  // Support both std::vector and mipp::vector (via template or implicit conv)
  template <typename Alloc>
  void encode(const std::vector<B, Alloc> &U_K, std::vector<B, Alloc> &X_N) {
    // LiteBCH returns vector, aff3ct writes to reference.
    // We convert/copy.

    // 1. Adapter: copy input to std::vector (if needed, or just use const ref)
    // std::vector cast is compatible for standard allocators.
    std::vector<B> msg(U_K.begin(), U_K.end());

    // 2. Perform
    std::vector<B> encoded = bch->encode(msg);

    // 3. Copy back
    if (X_N.size() != encoded.size())
      X_N.resize(encoded.size());
    std::copy(encoded.begin(), encoded.end(), X_N.begin());
  }

private:
  std::shared_ptr<lite::LiteBCH> bch;
};

// Mimics Decoder_BCH_std / Decoder_BCH_fast
template <typename B = int, typename R = float> class Decoder_BCH_std {
public:
  Decoder_BCH_std(int K, int N,
                  const tools::BCH_polynomial_generator<B> &poly_gen) {
    bch =
        std::make_shared<lite::LiteBCH>(N, poly_gen.get_t(), poly_gen.get_p());
  }

  // decode_hiho (Hard Input Hard Output)
  // Returns: Status (0 = success, !0 = fail)
  template <typename Alloc>
  int decode_hiho(const std::vector<B, Alloc> &Y_N,
                  std::vector<B, Alloc> &V_K) {
    // 1. Adapter
    std::vector<B> received(Y_N.begin(), Y_N.end());
    std::vector<B> decoded;

    // 2. Perform
    bool success = bch->decode(received, decoded);

    // 3. Copy back
    if (V_K.size() != decoded.size())
      V_K.resize(decoded.size());
    std::copy(decoded.begin(), decoded.end(), V_K.begin());

    return success ? 0 : 1; // 0 is success in aff3ct
  }

private:
  std::shared_ptr<lite::LiteBCH> bch;
};

// Alias Fast decoder to Std (LiteBCH is fast enough)
template <typename B = int, typename R = float>
using Decoder_BCH_fast = Decoder_BCH_std<B, R>;

} // namespace module
} // namespace aff3ct

#endif // AFF3CT_COMPAT_H
