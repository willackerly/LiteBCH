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

      // Raw Byte Buffer Encode (High Perf)
      // JS must manage memory (alloc/free) and pass pointers.
      .function("encode_raw_ptr", &encode_raw_ptrs)

      .property("ecc_bytes", &lite::LiteBCH::get_ecc_bytes)

      .function("decode", static_cast<bool (lite::LiteBCH::*)(
                              const std::vector<int> &, std::vector<int> &)>(
                              &lite::LiteBCH::decode));
}
