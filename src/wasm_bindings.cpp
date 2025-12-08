#include <emscripten/bind.h>
#include <litebch/LiteBCH.h>
#include <vector>

using namespace emscripten;

// Wrapper (or direct binding if signatures match)
// LiteBCH uses std::vector<int> for messages and encoded data.
// We need to register std::vector<int> so JS can use it.

// Helper to expose Byte-Wise Encode to JS via Vector Interface
void encode_bytes_wrapper(lite::LiteBCH &bch, val data_qs, int len,
                          val ecc_qs) {
  // Placeholder for direct memory binding if needed.
  // Currently unused as we rely on encode_fast_wrapper via vectors.
}

// Expose 'encode' taking integer memory addresses (pointers) to avoid copies.
// JS side must manage memory (alloc/free) and pass aligned pointers.
void encode_raw_ptrs(lite::LiteBCH &bch, uintptr_t data_ptr, int len,
                     uintptr_t ecc_ptr) {
  uint8_t *data = reinterpret_cast<uint8_t *>(data_ptr);
  uint8_t *ecc = reinterpret_cast<uint8_t *>(ecc_ptr);
  bch.encode(data, len, ecc);
}

// Factory for debug
lite::LiteBCH *create_litebch_custom(int N, int t, const std::vector<int> &p) {
  try {
    return new lite::LiteBCH(N, t, p);
  } catch (const std::exception &e) {
    throw;
  }
}

// Wrapper for Fast Encoding (Bit-Vector Interface)
// Adapts the byte-oriented fast encoder to the bit-oriented JS interface
std::vector<int> encode_fast_wrapper(lite::LiteBCH &bch,
                                     const std::vector<int> &msg_bits) {
  int K = bch.get_K();
  int N = bch.get_N();
  int ecc_bytes = bch.get_ecc_bytes();
  int n_rdncy = N - K;

  if (msg_bits.size() != (size_t)K) {
    throw std::invalid_argument("Message size must be K");
  }

  // 1. Pack Bits to Bytes (MSB First)
  // Matching LiteBCH::decode logic
  std::vector<uint8_t> data((K + 7) / 8, 0);
  for (int i = 0; i < K; ++i) {
    // Message bits are packed such that index 0 is high degree.
    int stream_pos = K - 1 - i;
    if (msg_bits[i]) {
      data[stream_pos / 8] |= (1 << (7 - (stream_pos % 8)));
    }
  }

  // 2. Run Fast Encode
  std::vector<uint8_t> ecc(ecc_bytes);
  bch.encode(data.data(), data.size(), ecc.data());

  // 3. Unpack ECC to Bits and Construct Codeword [Parity | Message]
  std::vector<int> codeword(N);

  // Parity (ECC)
  // decode() expects ecc bits at indices 0..n_rdncy-1
  // and packs them: ecc[i/8] |= (1 << (i%8)) -> LSB packing for ECC!
  for (int i = 0; i < n_rdncy; ++i) {
    if ((ecc[i / 8] >> (i % 8)) & 1) {
      codeword[i] = 1;
    } else {
      codeword[i] = 0;
    }
  }

  // Message
  for (int i = 0; i < K; ++i) {
    codeword[n_rdncy + i] = msg_bits[i];
  }

  return codeword;
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

      // Fast Encode Wrapper
      .function("encode_fast", &encode_fast_wrapper)

      // Raw Byte Buffer Encode (High Perf)
      // JS must manage memory (alloc/free) and pass pointers.
      .function("encode_raw_ptr", &encode_raw_ptrs)

      .property("ecc_bytes", &lite::LiteBCH::get_ecc_bytes)

      .function("decode", static_cast<bool (lite::LiteBCH::*)(
                              const std::vector<int> &, std::vector<int> &)>(
                              &lite::LiteBCH::decode));
}
