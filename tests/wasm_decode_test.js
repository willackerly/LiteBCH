const fs = require('fs');
const path = require('path');

// Locate the build-wasm directory
const wasmPath = path.resolve(__dirname, '../build-wasm/litebch.js');

if (!fs.existsSync(wasmPath)) {
    console.error(`Error: Could not find WASM build at ${wasmPath}`);
    process.exit(1);
}

const createLiteBCH = require(wasmPath);

// --- LCG Logic (Must match repro_common.h) ---
class SimpleLCG {
    constructor(seed) {
        this.state = seed >>> 0;
    }
    next() {
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

createLiteBCH().then(Module => {
    console.log("LiteBCH WASM Module Loaded for Decode Test.");

    const N = 31;
    const t = 3;

    const bch = new Module.LiteBCH(N, t);
    const K = bch.get_K();
    console.log(`Initialized BCH(N=${N}, t=${t}, K=${K})`);

    const lcg = new SimpleLCG(42);
    const vectors = 5; // Test 5 vectors

    // JS Input Vector
    const messageVec = new Module.IntVector();
    messageVec.resize(K, 0);

    let passedVectors = 0;

    for (let v = 0; v < vectors; ++v) {
        // Fill message
        for (let i = 0; i < K; ++i) {
            messageVec.set(i, lcg.next_bit());
        }

        // Encode -> Encoded is IntVector (copy)
        const encoded = bch.encode(messageVec);

        // --- 1. Clean Decode Check ---
        const decodedClean = new Module.IntVector();
        // Note: decode takes (input, output_ref). JS binding might need help resizing output? 
        // In C++, output is resized. In JS, if mapped to reference, it should work.

        // Wait, did I resize decodedClean? C++ does it.
        // Let's pass empty vector.

        let success = bch.decode(encoded, decodedClean);
        if (!success) {
            console.error(`[FAIL] Clean decode returned false for vector ${v}`);
            process.exit(1);
        }

        // Verify Content
        let match = true;
        if (decodedClean.size() !== K) {
            console.error(`[FAIL] Clean decode size mismatch. Expected ${K}, got ${decodedClean.size()}`);
            match = false;
        } else {
            for (let i = 0; i < K; ++i) {
                if (decodedClean.get(i) !== messageVec.get(i)) {
                    match = false;
                    break;
                }
            }
        }

        if (!match) {
            console.error(`[FAIL] Clean decode content mismatch for vector ${v}`);
            process.exit(1);
        }

        // --- 2. Error Decode Check ---
        // Inject 3 errors at pos 0, 10, 20
        const corrupted = new Module.IntVector();
        // Copy encoded manually since IntVector copy ctor might not be exposed
        corrupted.resize(encoded.size(), 0);
        for (let i = 0; i < encoded.size(); ++i) corrupted.set(i, encoded.get(i));

        // Flip bits
        corrupted.set(0, corrupted.get(0) ^ 1);
        corrupted.set(10, corrupted.get(10) ^ 1);
        corrupted.set(20, corrupted.get(20) ^ 1);

        const decodedErr = new Module.IntVector();
        success = bch.decode(corrupted, decodedErr);

        if (!success) {
            console.error(`[FAIL] Error decode returned false for vector ${v} (3 errors)`);
            process.exit(1);
        }

        match = true;
        for (let i = 0; i < K; ++i) {
            if (decodedErr.get(i) !== messageVec.get(i)) {
                match = false;
                break;
            }
        }

        if (!match) {
            console.error(`[FAIL] Error decode content mismatch for vector ${v}`);
            process.exit(1);
        }

        encoded.delete();
        decodedClean.delete();
        corrupted.delete();
        decodedErr.delete();

        passedVectors++;
    }

    messageVec.delete();
    bch.delete();

    console.log(`PASS: All ${passedVectors} vectors decoded successfully (Clean & Corrupted).`);
});
