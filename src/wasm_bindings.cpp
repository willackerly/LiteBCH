#include <emscripten/bind.h>
#include <litebch/LiteBCH.h>
#include <vector>

using namespace emscripten;

// Wrapper (or direct binding if signatures match)
// LiteBCH uses std::vector<int> for messages and encoded data.
// We need to register std::vector<int> so JS can use it.

// Helper to expose Byte-Wise Encode to JS via Vector Interface
// (JS deals with IntVectors of bits usually in this API)
// Direct Byte-Buffer Interface for High Performance in JS
// Accepts Uint8Array from JS via val
void encode_bytes_wrapper(lite::LiteBCH &bch, val data_qs, int len,
                          val ecc_qs) {
  // 1. Get raw pointers into WASM memory from the typed arrays
  // NOTE: This assumes the JS side passes HEAPU8 subarrays or pointers
  // directly? Embind is safer with 'std::string' or 'val'. Best practice for
  // bulk data: Intecept memory view or copy. For simplicity/safety, we'll copy
  // in/out using std::vector<uint8_t> binding? Actually, passing 'uintptr_t'
  // pointers from JS (Module.HEAU8) is the fastest way "C-style".

  // Let's implement the safely exposed version that takes integer pointers
  // (offsets) The JS side will use `alloc()` to get these pointers.

  // Re-signature to take pointers as integers
  // JS: _encode_bytes(bch, ptr_data, len, ptr_ecc)
  // But we are in a class method context.

  // We can't change the binding easily without standard types.
  // Let's rely on std::string as a byte container (binary string) which Embind
  // supports well, OR just stick to standard vector<uint8_t> which Embind
  // supports if registered.
}

// Better Approach: Expose 'encode' taking integer memory addresses (pointers).
// This allows JS to malloc inputs/outputs and pass offsets.
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

  // 1. Pack Bits to Bytes (MSB First / High Degree First)
  // Matching LiteBCH::decode logic
  std::vector<uint8_t> data((K + 7) / 8, 0);
  for (int i = 0; i < K; ++i) {
    // Message bits are 0..K-1.
    // In our legacy/standard layout, these are high degree polynomials.
    // We pack them such that the first byte contains the highest degree terms.
    // Actually, let's follow the standard big-endian bit packing used in
    // networking, which usually matches the polynomial order if bit 0 is top.
    // Let's emulate what decode dies:
    // int stream_pos = K - 1 - i;
    // data[stream_pos / 8] |= (1 << (7 - (stream_pos % 8)));
    // BUT encoding usually takes data directly.
    // LiteBCH::encode (fast) expects standard byte array.
    // Let's just pack conventionally.
    // If msg_bits[0] is the first bit of the message...
    // We pack 8 bits into a byte.
    // But we need to be careful about bit endianness vs polynomial order.
    // Legacy __encode uses bitwise LFSR on U_K[i] (High Degree down to 0).
    // Fast encode iterates bytes.
    // We'll trust that packing 0..7 into byte 0 (MSB..LSB) is correct for the
    // fast encoder if it was written to match standard comms.
    //
    // Actually, looking at decode():
    // it maps received_bits[n_rdncy + i] (message part) to data bytes.
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
