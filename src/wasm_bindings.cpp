#include <emscripten/bind.h>
#include <litebch/LiteBCH.h>
#include <vector>

using namespace emscripten;

// Wrapper (or direct binding if signatures match)
// LiteBCH uses std::vector<int> for messages and encoded data.
// We need to register std::vector<int> so JS can use it.

EMSCRIPTEN_BINDINGS(litebch_module) {
  // Register std::vector<int> as "IntVector" in JS
  register_vector<int>("IntVector");

  class_<lite::LiteBCH>("LiteBCH")
      .constructor<int, int>()
      .function("get_K", &lite::LiteBCH::get_K)
      // Resolve overload: std::vector<int> encode(const std::vector<int>&)
      .function("encode",
                static_cast<std::vector<int> (lite::LiteBCH::*)(
                    const std::vector<int> &)>(&lite::LiteBCH::encode))
      // decode returns bool, and takes (input, output_ref).
      // Embind handles references well, but for cleaner JS API we might want to
      // return the vector or an object. However, keeping it 1:1 for now is
      // simplest. In JS: var dec_out = new Module.IntVector(); var success =
      // bch.decode_ref(enc, dec_out); Note: We need to define a wrapper if we
      // want to return the vector directly, but let's try binding the standard
      // signature first. Actually, reference parameters in JS are tricky
      // without pointers. Let's create a helper for decode that returns a
      // struct or the vector directly (throws on error?). Or just binding
      // `decode` might work if we pass an instance of IntVector.
      .function("decode", &lite::LiteBCH::decode);
}
