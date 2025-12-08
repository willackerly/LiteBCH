# BCHLight Tests

This directory contains the testing and verification suite for BCHLight.

## Core Tests

### 1. `comprehensive_test.cpp` (Primary Verification)
**Target:** `comprehensive_test`
The main verification suite for the library. It tests:
- **Correctness:** Verifies encoding/decoding against known golden checksums.
- **Performance:** measures Mbps for both bitwise (legacy) and fast byte-oriented APIs.
- **Consistency:** Ensures both APIs produce identical results.
- **WASM Compatibility:** Can verify WASM builds if configured.

**Usage:**
```bash
./comprehensive_test
```

### 2. `unit_tests.cpp` (Sanity Check)
**Target:** `unit_tests`
A lightweight set of unit tests for quick sanity checking during development. It covers basic encoding, decoding, and error correction scenarios manually.

**Usage:**
```bash
./unit_tests
```

## Benchmarks

### `kernel_bench.cpp` (Linux Kernel Comparison)
**Target:** `kernel_bch`
A specialized benchmark that compares `LiteBCH` performance against the Linux Kernel's `bch.c` implementation (included in `external/`). This is useful for proving the performance claims of the library.

**Usage:**
```bash
./kernel_bch
```

## WASM Tests

The `*.js` files in this directory are used to verify the WebAssembly build. They are typically invoked via the `comprehensive_test` runner or during CI.

- `wasm_test.js`: Basic load and encode test.
- `wasm_decode_test.js`: Verifies decoding logic in JS.
- `wasm_comprehensive_test.js`: A JS port of the comprehensive test logic to verify the WASM artifact in a real JS environment.

## Build Configuration

Tests are built using CMake. By default, `aff3ct` verification is disabled to keep the build light.

To run tests:
```bash
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
make
# Run specific tests
./tests/comprehensive_test
```
