const fs = require('fs');
const factory = require('../build_wasm/litebch.js');

// --- Configuration ---
const configs = [
    { name: "Small", m: 5, t: 3, p: [] },
    { name: "Medium", m: 10, t: 50, p: [] },
    { name: "Medium-C", m: 10, t: 50, p: [1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1] }, // x^10 + x^3 + 1
    { name: "Large", m: 13, t: 60, p: [] },
    { name: "Large-C", m: 13, t: 60, p: [1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1] }, // x^13 + x^4 + x^3 + x + 1
    { name: "X-Large", m: 14, t: 120, p: [] },
    { name: "XX-Large", m: 15, t: 140, p: [] }
];

const vectors_per_config = 100;

// Expected Checksums (Must match C++ comprehensive_test.cpp)
const expected_checksums = {
    "Small": 0x64b1f50a,
    "Medium": 0x55dcc166,
    "Medium-C": 0x2d6be2d9,
    "Large": 0x5f255101,
    "Large-C": 0x5f255101,
    "X-Large": 0x74920925,
    "XX-Large": 0x4054b9e4
};

// --- LCG RNG (Replicating C++ Logic) ---
class LCG {
    constructor(seed) {
        this.state = seed >>> 0;
    }
    next() {
        // state = state * 1664525 + 1013904223;
        // JS Integers are safe up to 53 bits. 32-bit mul can overflow 53 bits?
        // 1664525 * 0xFFFFFFFF approx 7e15, which is < 9e15 (MAX_SAFE_INTEGER).
        // So standard double precision math is fine, then truncate.
        this.state = ((this.state * 1664525) + 1013904223) >>> 0;
        return this.state;
    }
    next_bit() {
        return (this.next() >>> 31) & 1;
    }
}

// --- CRC32 (Replicating C++ Logic) ---
function crc32_vec(data_bits) {
    let hash = 0;
    for (let bit of data_bits) {
        // hash = (hash << 5) ^ (hash >> 27) ^ bit;
        // In C++, >> on unsigned is logical shift. JS >>> is logical shift.
        // JS << 5 produces 32-bit signed int.
        hash = ((hash << 5) ^ (hash >>> 27) ^ bit) >>> 0;
    }
    return hash;
}

factory().then(Module => {
    const is_csv = process.argv.includes('--csv');

    if (!is_csv) {
        console.log("------------------------------------------");
        console.log(" LiteBCH WASM Comprehensive Verification");
        console.log("------------------------------------------");
    }

    let all_passed = true;

    for (const cfg of configs) {
        let checksum = 0;
        const N = (1 << cfg.m) - 1;

        // Initialize BCH
        let bch;
        try {
            if (cfg.p.length > 0) {
                // Convert JS array to IntVector
                const vec_p = new Module.IntVector();
                for (let v of cfg.p) vec_p.push_back(v);
                bch = new Module.LiteBCH(N, cfg.t, vec_p);
                vec_p.delete();
            } else {
                bch = new Module.LiteBCH(N, cfg.t);
            }
        } catch (e) {
            if (!is_csv) console.error(`[FAIL] ${cfg.name}: Init failed - ${e}`);
            all_passed = false;
            continue;
        }

        const K = bch.get_K(); // Function
        const ecc_bytes_len = bch.ecc_bytes; // Property
        const data_bytes_len = Math.ceil(K / 8);

        // Prepare LCG
        const lcg = new LCG(12345 + cfg.m);

        // Pre-allocate WASM memory for one vector
        const data_ptr = Module._malloc(data_bytes_len);
        const ecc_ptr = Module._malloc(ecc_bytes_len);

        // Loop vectors
        for (let v = 0; v < vectors_per_config; ++v) {
            // Generate Message Bits
            const msg_bits = new Int32Array(K);
            for (let i = 0; i < K; ++i) msg_bits[i] = lcg.next_bit();

            // Pack Bits to Bytes (MSB first)
            const data_view = new Uint8Array(Module.HEAPU8.buffer, data_ptr, data_bytes_len);
            data_view.fill(0);
            for (let i = 0; i < K; ++i) {
                if (msg_bits[i]) {
                    const pos = K - 1 - i;
                    const byte_idx = Math.floor(pos / 8);
                    const bit_idx = 7 - (pos % 8);
                    data_view[byte_idx] |= (1 << bit_idx);
                }
            }

            // Encode (High Perf API)
            // Signature: encode_raw_ptr(bch, data_ptr, len, ecc_ptr)
            bch.encode_raw_ptr(data_ptr, data_bytes_len, ecc_ptr);

            // Read ECC Bytes
            const ecc_view = new Uint8Array(Module.HEAPU8.buffer, ecc_ptr, ecc_bytes_len);

            // Reconstruct Codeword [Parity | Message]
            // Unpack ECC (LSB first)
            const n_red = N - K;
            const codeword_bits = new Int32Array(N);

            for (let i = 0; i < n_red; ++i) {
                const byte_idx = Math.floor(i / 8);
                const bit_idx = i % 8;
                if (ecc_view[byte_idx] & (1 << bit_idx)) {
                    codeword_bits[i] = 1;
                }
            }

            // Append Message
            for (let i = 0; i < K; ++i) {
                codeword_bits[n_red + i] = msg_bits[i];
            }

            // Update Checksum
            checksum = (checksum ^ crc32_vec(codeword_bits)) >>> 0;
        }

        // Verify
        const expected = expected_checksums[cfg.name];
        let status = "FAIL";
        if ((checksum >>> 0) === (expected >>> 0)) {
            status = "PASS";
        } else {
            status = "FAIL";
            all_passed = false;
        }

        if (is_csv) {
            console.log(`${cfg.name},${(checksum >>> 0).toString(16)},${status}`);
        } else {
            if (status === "PASS") {
                console.log(`| ${cfg.name.padEnd(10)} | Checksum: ${(checksum >>> 0).toString(16)} | ${status}`);
            } else {
                console.error(`| ${cfg.name.padEnd(10)} | Checksum: ${(checksum >>> 0).toString(16)} (Exp: ${expected.toString(16)}) | ${status}`);
            }
        }

        // Cleanup
        Module._free(data_ptr);
        Module._free(ecc_ptr);
        bch.delete();
    }

    if (!all_passed) process.exit(1);
});
