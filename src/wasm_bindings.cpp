#include <emscripten/bind.h>
#include <litebch/LiteBCH.h>
#include <vector>

using namespace emscripten;

// Wrapper (or direct binding if signatures match)
// LiteBCH uses std::vector<int> for messages and encoded data.
// We need to register std::vector<int> so JS can use it.

// Helper to expose Byte-Wise Encode to JS via Vector Interface
// (JS deals with IntVectors of bits usually in this API)
std::vector<int> encode_fast_wrapper(lite::LiteBCH &bch,
                                     const std::vector<int> &msg_bits) {
  int K = bch.get_K();
  int N = bch.get_N();
  if (msg_bits.size() != (size_t)K)
    throw std::runtime_error("Invalid K");

  // Pack bits to bytes (MSB first)
  int data_bytes_len = (K + 7) / 8;
  std::vector<uint8_t> data(data_bytes_len, 0);
  for (int i = 0; i < K; ++i) {
    if (msg_bits[i]) {
      int pos_from_start = (K - 1 - i);
      data[pos_from_start / 8] |= (1 << (7 - (pos_from_start % 8)));
    }
  }

  // Encode
  int ecc_bytes_len = (N - K + 7) / 8;
  std::vector<uint8_t> ecc(ecc_bytes_len, 0);
  bch.encode(data.data(), data_bytes_len, ecc.data());

  // Reconstruct Codeword [Parity | Message] to match Decoder/Legacy
  std::vector<int> codeword(N, 0);

  // ECC
  int n_red = N - K;
  for (int i = 0; i < n_red; ++i) {
    if (ecc[i / 8] & (1 << (i % 8)))
      codeword[i] = 1;
  }
  // Msg
  for (int i = 0; i < K; ++i) {
    if (msg_bits[i])
      codeword[n_red + i] = 1;
  }
  return codeword;
}

// Factory for debug
lite::LiteBCH *create_litebch_custom(int N, int t, const std::vector<int> &p) {
  try {
    return new lite::LiteBCH(N, t, p);
  } catch (const std::exception &e) {
    throw;
  }
}

EMSCRIPTEN_BINDINGS(litebch_module) {
  register_vector<int>("IntVector");

  class_<lite::LiteBCH>("LiteBCH")
      .constructor<int, int>()
      // Use factory for custom poly
      .constructor(&create_litebch_custom, allow_raw_pointers())
      .function("get_K", &lite::LiteBCH::get_K)
      .function("get_N", &lite::LiteBCH::get_N) // Exposure needed
      .function("get_t", &lite::LiteBCH::get_t) // Exposure needed

      // Legacy Encode
      .function("encode",
                static_cast<std::vector<int> (lite::LiteBCH::*)(
                    const std::vector<int> &)>(&lite::LiteBCH::encode))

      // Fast Encode (Wrapped)
      .function("encode_fast", &encode_fast_wrapper)

      .function("decode", &lite::LiteBCH::decode);
}
