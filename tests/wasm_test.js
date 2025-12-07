const fs = require('fs');
const path = require('path');

// Locate the build-wasm directory
// Adjust relative path as needed. Assuming running from root: node tests/wasm_test.js
const wasmPath = path.resolve(__dirname, '../build-wasm/litebch.js');

if (!fs.existsSync(wasmPath)) {
    console.error(`Error: Could not find WASM build at ${wasmPath}`);
    console.error("Please build with: emcmake cmake -B build-wasm -DLITEBCH_BUILD_WASM=ON && cmake --build build-wasm");
    process.exit(1);
}

const createLiteBCH = require(wasmPath);

// --- LCG Logic (Must match repro_common.h) ---
class SimpleLCG {
    constructor(seed) {
        this.state = seed >>> 0; // uint32
    }

    next() {
        // state = (1103515245 * state + 12345) & 0x7FFFFFFF;
        // JS Numbers are doubles. High precision integer math needs BigInt or careful handling.
        // But 32-bit LCG fits in 53-bit integer safety of double.
        // 1103515245 * 2^31 is ~2.3e18, which exceeds standard float precision? No.
        // 1103515245 * 2147483647 ~= 2.37e18. 
        // Max safe integer is 9e15. 
        // So direct multiply might lose precision. Use BigInt for safety.

        const a = 1103515245n;
        const c = 12345n;
        const m = 0x7FFFFFFFn;

        let s = BigInt(this.state);
        s = (a * s + c) & m;
        this.state = Number(s);
        return this.state;
    }

    next_bit() {
        return this.next() % 2;
    }
}

// Checksum (DJB2-like)
function calculate_checksum(data) {
    // data is IntVector (from WASM) or Array
    let hash = 5381n;

    // Iterate manually to support both Array and WASM Vector
    for (let i = 0; i < data.size(); ++i) {
        let val = BigInt(data.get(i));
        // hash = ((hash << 5) + hash) + val
        hash = ((hash << 5n) + hash) + val;

        // Emulate 32-bit overflow
        hash = hash & 0xFFFFFFFFn;
    }
    return Number(hash);
}

// --- Main Test ---
const GOLDEN_CHECKSUM = 3117751785;

createLiteBCH().then(Module => {
    console.log("LiteBCH WASM Module Loaded.");

    const N = 255;
    const t = 10;

    const bch = new Module.LiteBCH(N, t);
    const K = bch.get_K();
    console.log(`Initialized BCH(N=${N}, t=${t}, K=${K})`);

    const lcg = new SimpleLCG(42);
    const vectors = 10;
    let total_checksum = 0n;

    // JS Input Vector
    const messageVec = new Module.IntVector();
    // Pre-allocate (resize)
    messageVec.resize(K, 0);

    for (let v = 0; v < vectors; ++v) {
        // Fill message
        for (let i = 0; i < K; ++i) {
            messageVec.set(i, lcg.next_bit());
        }

        // Encode
        // Returns a copy (IntVector)
        const encoded = bch.encode(messageVec);

        const chk = calculate_checksum(encoded);
        total_checksum = (total_checksum ^ BigInt(chk)) & 0xFFFFFFFFn;

        encoded.delete();
    }

    messageVec.delete();
    bch.delete();

    const actual = Number(total_checksum);
    console.log(`Calculated Checksum: ${actual}`);
    console.log(`Expected Checksum:   ${GOLDEN_CHECKSUM}`);

    if (actual === GOLDEN_CHECKSUM) {
        console.log("PASS: Checksums match.");
        process.exit(0);
    } else {
        console.error("FAIL: Checksum mismatch!");
        process.exit(1);
    }
});
