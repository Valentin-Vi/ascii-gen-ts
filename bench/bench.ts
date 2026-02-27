import * as fs from 'fs';
import * as path from 'path';
import { convertFrame } from '../src/converter';
import { convertFrameNative } from '../src/native';

// ── Synthetic frame ────────────────────────────────────────────────────────────
const FRAME_W = 640;
const FRAME_H = 480;
const COLS = 80;
const ROWS = 40;

function makeGradientFrame(w: number, h: number): Uint8Array {
  const buf = new Uint8Array(w * h * 4);
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const off = (y * w + x) * 4;
      buf[off + 0] = Math.round((x / (w - 1)) * 255);        // R: left→right
      buf[off + 1] = Math.round((y / (h - 1)) * 255);        // G: top→bottom
      buf[off + 2] = Math.round(((x + y) / (w + h - 2)) * 255); // B: diagonal
      buf[off + 3] = 255;
    }
  }
  return buf;
}

// ── Benchmarking helpers ───────────────────────────────────────────────────────
function percentile(sorted: bigint[], p: number): bigint {
  const idx = Math.min(Math.ceil((p / 100) * sorted.length) - 1, sorted.length - 1);
  return sorted[idx];
}

interface Stats {
  min: bigint;
  max: bigint;
  mean: bigint;
  p50: bigint;
  p95: bigint;
  samples: bigint[];
}

function measure(fn: () => void, warmup: number, iters: number): Stats {
  for (let i = 0; i < warmup; i++) fn();

  const samples: bigint[] = [];
  for (let i = 0; i < iters; i++) {
    const t0 = process.hrtime.bigint();
    fn();
    samples.push(process.hrtime.bigint() - t0);
  }

  const sorted = [...samples].sort((a, b) => (a < b ? -1 : a > b ? 1 : 0));
  const sum = samples.reduce((a, b) => a + b, 0n);

  return {
    min: sorted[0],
    max: sorted[sorted.length - 1],
    mean: sum / BigInt(samples.length),
    p50: percentile(sorted, 50),
    p95: percentile(sorted, 95),
    samples,
  };
}

// ── Run ────────────────────────────────────────────────────────────────────────
const WARMUP = 100;
const ITERS = 500;

console.log(`Benchmark: ${FRAME_W}×${FRAME_H} RGBA → ${COLS}×${ROWS} ASCII`);
console.log(`Warm-up: ${WARMUP}  Measured: ${ITERS}\n`);

const pixels = makeGradientFrame(FRAME_W, FRAME_H);
const opts = { cols: COLS, rows: ROWS };

console.log('Running pure-TS...');
const tsStats = measure(() => convertFrame(pixels, FRAME_W, FRAME_H, opts), WARMUP, ITERS);

console.log('Running native (C)...');
const nativeStats = measure(
  () => convertFrameNative(pixels, FRAME_W, FRAME_H, opts),
  WARMUP,
  ITERS,
);

// ── Correctness check ──────────────────────────────────────────────────────────
const tsOut = convertFrame(pixels, FRAME_W, FRAME_H, opts);
const nativeOut = convertFrameNative(pixels, FRAME_W, FRAME_H, opts);

const tsChar = tsOut[0][0].char;
const nativeChar = nativeOut[0][0].char;

if (tsChar !== nativeChar) {
  console.warn(`WARNING: [0][0].char mismatch: TS='${tsChar}' native='${nativeChar}'`);
} else {
  console.log(`\nCorrectness: [0][0].char = '${tsChar}' (match)\n`);
}

// ── Table ──────────────────────────────────────────────────────────────────────
function fmt(ns: bigint): string {
  return (Number(ns) / 1_000).toFixed(1).padStart(10) + ' µs';
}

const speedup = Number(tsStats.mean) / Number(nativeStats.mean);

console.log('┌─────────────┬────────────┬────────────┬────────────┬────────────┬────────────┐');
console.log('│ impl        │    min     │    mean    │    p50     │    p95     │    max     │');
console.log('├─────────────┼────────────┼────────────┼────────────┼────────────┼────────────┤');
console.log(
  `│ pure-TS     │${fmt(tsStats.min)} │${fmt(tsStats.mean)} │${fmt(tsStats.p50)} │${fmt(tsStats.p95)} │${fmt(tsStats.max)} │`,
);
console.log(
  `│ native (C)  │${fmt(nativeStats.min)} │${fmt(nativeStats.mean)} │${fmt(nativeStats.p50)} │${fmt(nativeStats.p95)} │${fmt(nativeStats.max)} │`,
);
console.log('└─────────────┴────────────┴────────────┴────────────┴────────────┴────────────┘');
console.log(`\nSpeedup (mean): ${speedup.toFixed(2)}×  (native is ${speedup >= 1 ? 'faster' : 'slower'})`);

// ── Write JSON ─────────────────────────────────────────────────────────────────
interface SerializableStats {
  min_ns: string;
  max_ns: string;
  mean_ns: string;
  p50_ns: string;
  p95_ns: string;
  samples_ns: string[];
}

function statsToJson(s: Stats): SerializableStats {
  return {
    min_ns: s.min.toString(),
    max_ns: s.max.toString(),
    mean_ns: s.mean.toString(),
    p50_ns: s.p50.toString(),
    p95_ns: s.p95.toString(),
    samples_ns: s.samples.map((n) => n.toString()),
  };
}

const results = {
  config: {
    frame_w: FRAME_W,
    frame_h: FRAME_H,
    cols: COLS,
    rows: ROWS,
    warmup: WARMUP,
    iters: ITERS,
  },
  correctness: {
    ts_char_0_0: tsChar,
    native_char_0_0: nativeChar,
    match: tsChar === nativeChar,
  },
  summary: {
    speedup_mean: speedup,
    faster: speedup >= 1 ? 'native' : 'pure-ts',
  },
  pure_ts: statsToJson(tsStats),
  native: statsToJson(nativeStats),
};

const outPath = path.join(__dirname, '..', 'bench-results.json');
fs.writeFileSync(outPath, JSON.stringify(results, null, 2));
console.log(`\nResults written to ${outPath}`);
