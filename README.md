# LiteBCH: High-Performance, Standalone BCH Error Correction

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Standard](https://img.shields.io/badge/C%2B%2B-11-blue.svg)](https://en.cppreference.com/w/cpp/11)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![Correctness](https://img.shields.io/badge/verified-bit%20exact-green.svg)]()

**LiteBCH** is a modern C++ library for Bose–Chaudhuri–Hocquenghem (BCH) error correction. It combines the **mathematical correctness** of the `aff3ct` library with the **performance methodologies** of the Linux Kernel, all in a single, dependency-free header.

## 🚀 Why LiteBCH?

*   **Fast by Default**: Uses **8-bit Parallel Look-Up Tables** (LUT) to achieve ~850 Mbps encoding throughput (19x faster than standard bit-serial implementations).
*   **Micro Footprint**: The entire library is **~570 lines of code**. Compare that to `aff3ct`'s **200,000+ lines** and dozens of files. It compiles in milliseconds.
*   **Safe & Portable**: Unlike the Linux Kernel implementation, LiteBCH is **endian-neutral** and **alignment-safe**. It runs on any architecture (x86, ARM, RISC-V, WASM) without crashing on unaligned memory access.
*   **Production Verified**: Validated bit-for-bit against `aff3ct` across 10,000+ test vectors.
*   **Zero Dependencies**: No Boost, no MIPP, no build scripts. Just `#include <litebch/LiteBCH.h>`.
*   **Flexible**: Supports any codeword size from $N=31$ to $N=32767+$ and **Custom Polynomials**.

---

## ⚡️ Performance

LiteBCH implements a "Table-Driven LFSR" approach similar to the Linux Kernel, but optimized for stability and cache efficiency.

### Benchmark (Apple M3 Max, N=32,767)

| Implementation | Architecture | Throughput | Speedup | Dependencies |
| :--- | :--- | :--- | :--- | :--- |
| **Old LiteBCH / aff3ct** | Bit-Serial LFSR | ~43 Mbps | 1x | Heavy (aff3ct) |
| **LiteBCH (New)** | **8-bit Parallel LUT** | **~850 Mbps** | **19.3x** | **None** |
| Linux Kernel Port | 32-bit Parallel LUT | ~2800 Mbps | 65x | Complex / C-Only |

### Design Philosophy: Why 8-bit Parallelism?
While the Linux Kernel uses 32-bit (Slice-by-4) parallelism, LiteBCH intentionally uses **8-bit Parallelism**:

1.  **Safety**: 32-bit word access requires strict memory alignment. Reading a `uint32_t` from address `0x1001` crashes many ARM/RISC-V CPUs. LiteBCH handles bytes, making it safe everywhere.
2.  **Portability**: Casting bytes to integers involves Endianness (Byte Order) headaches. 8-bit processing is mathematically identical on Big and Little Endian systems.
3.  **Cache Efficiency**: Our 8-bit tables fit in **~10KB** L1 Cache. 32-bit tables require **40KB+**, which can cause cache thrashing in embedded real-time systems.

---

## 📦 Usage

### 1. Fast Byte-Oriented API (Recommended)
The fastest way to use LiteBCH. Works directly on your binary data buffers.

```cpp
#include <litebch/LiteBCH.h>

// Initialize: N=1023 bits, Correct up to 50 errors
lite::LiteBCH bch(1023, 50); 

// Data Buffers
std::vector<uint8_t> data(bch.get_K() / 8); 
std::vector<uint8_t> ecc(bch.ecc_bytes);

// Fast Encode (High Throughput)
bch.encode(data.data(), data.size(), ecc.data());
```

### 2. Legacy / Bit-Oriented API
Compatible with `aff3ct` style vectors of bits (0/1 integers). Useful for simulation pipelines.

```cpp
std::vector<int> message_bits = ...; // 0 or 1
std::vector<int> codeword = bch.encode(message_bits);
```

### 3. Custom Polynomials (Flexibility)
Need to interface with legacy hardware or satellite standards (DVB-S2)? You can specify the Primitive Polynomial.

```cpp
// Example: N=1023, t=50 with polynomial x^10 + x^3 + 1
std::vector<int> p = {1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1};
lite::LiteBCH bch(1023, 50, p);
```

---

## 🛠 Integration

### Method 1: Drop-In
Copy `include/litebch` and `src/LiteBCH.cpp` into your project. That's it.

### Method 2: CMake
```bash
add_subdirectory(litebch)
target_link_libraries(your_app PRIVATE litebch::litebch)
```

### Method 3: WebAssembly (WASM)
You can compile LiteBCH for the web using Emscripten.

```bash
# Configure with Emscripten
emcmake cmake -B build-wasm -DLITEBCH_BUILD_WASM=ON

# Build
cmake --build build_wasm

# Output: build-wasm/litebch.js and build-wasm/litebch.wasm
```

### 4. Interactive Web Demo (Verified)
We include a robust browser-based demo in `examples/web_demo.html`.
- **Features**: Configurable N/t, Error Injection, and **In-Browser Supertest**.
- **Run**:
  ```bash
  python3 -m http.server
  # Open http://localhost:8000/examples/web_demo.html
  ```

### 5. WASM Verification Suite
We don't just compile to WASM; we **verify** it bit-for-bit against the C++ Legacy implementation using `tests/wasm_supertest.js`.
- **Coverage**: Small ($N=31$) to XX-Large ($N=8191$) + Custom Polynomials.
- **Checks**:
    - **Encoder Consistency**: `encode_fast()` (Byte-Wise) == `encode()` (Legacy Bit-Wise).
    - **Decoder Correctness**: Recovers from $t$ errors.
    - **Boundary Safety**: Fails gracefuly (or detects mismatch) on $t+1$ errors.
    - **Regression Safety**: Hardcoded Golden Checksums for every config.
- **Run**: `node tests/wasm_supertest.js`

## 🔄 Migration from aff3ct

If you are using `aff3ct`, you can switch to LiteBCH to remove >100,000 lines of dependencies while keeping a similar API.

| Feature | aff3ct | LiteBCH |
| :--- | :--- | :--- |
| **Header** | `#include <aff3ct.hpp>` | `#include <litebch/LiteBCH.h>` |
| **Init** | 3 Classes (Poly, Enc, Dec) | 1 Class (`LiteBCH`) |
| **API** | Complex Module System | Simple Functions |

---

## License
MIT License. Free for commercial and private use.
