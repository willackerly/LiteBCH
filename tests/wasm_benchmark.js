const fs = require('fs');
const path = require('path');
const factory = require('../build_wasm/litebch.js');

// Configs matching C++ benchmark
const CONFIGS = [
    { name: "Small", m: 5, t: 3, poly: [] },
    { name: "Medium", m: 8, t: 10, poly: [] },
    { name: "Large", m: 10, t: 50, poly: [1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1] },
    { name: "X-Large", m: 12, t: 20, poly: [] },
    { name: "XX-Large", m: 13, t: 40, poly: [] }
];

async function runBenchmark() {
    const Module = await factory();

    console.log("=========================================================================");
    console.log("| Config   | m  | N    | t  | K    | Vect | Total Time | Throughput |");
    console.log("|----------|----|------|----|------|------|------------|------------|");

    for (const cfg of CONFIGS) {
        const N = (1 << cfg.m) - 1;
        let bch = null;

        try {
            if (cfg.poly.length > 0) {
                const pVec = new Module.IntVector();
                for (let x of cfg.poly) pVec.push_back(x);
                bch = new Module.LiteBCH(N, cfg.t, pVec);
                pVec.delete();
            } else {
                bch = new Module.LiteBCH(N, cfg.t);
            }

            const K = bch.get_K();
            const vectors = 1000;

            // Generate Data
            const messages = [];
            for (let v = 0; v < vectors; ++v) {
                const msg = new Module.IntVector();
                for (let i = 0; i < K; ++i) msg.push_back(Math.random() > 0.5 ? 1 : 0);
                messages.push(msg);
            }

            // Benchmark Encode
            const start = performance.now();
            let totalBits = 0;
            for (let v = 0; v < vectors; ++v) {
                const cw = bch.encode_fast(messages[v]);
                totalBits += K; // Throughput usually based on Message Bits or Codeword bits?
                // C++ benchmark doesn't explicit compute throughput, just time.
                // But usually throughput = (Vectors * K) / Time
                cw.delete();
            }
            const end = performance.now();
            const durationMs = end - start;
            const durationSec = durationMs / 1000.0;
            const totalBitsProcessed = vectors * K;
            const mbps = (totalBitsProcessed / 1e6) / durationSec;

            console.log(
                `| ${cfg.name.padEnd(8)} | ` +
                `${cfg.m.toString().padEnd(2)} | ` +
                `${N.toString().padEnd(4)} | ` +
                `${cfg.t.toString().padEnd(2)} | ` +
                `${K.toString().padEnd(4)} | ` +
                `${vectors.toString().padEnd(4)} | ` +
                `${durationMs.toFixed(2).padStart(10)} | ` +
                `${mbps.toFixed(2).padStart(8)} M |`
            );

            // Cleanup
            for (let msg of messages) msg.delete();
            bch.delete();

        } catch (e) {
            console.log(`| ${cfg.name.padEnd(8)} | ERROR: ${e}`);
        }
    }
    console.log("=========================================================================");
}

runBenchmark();
