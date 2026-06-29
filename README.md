[![Release (crates.io)](https://github.com/fsbondtec/curvepress-rust/actions/workflows/release-crates.yml/badge.svg)](https://github.com/fsbondtec/curvepress-rust/actions/workflows/release-crates.yml)
[![Release (npm)](https://github.com/fsbondtec/curvepress-rust/actions/workflows/release-npm.yml/badge.svg)](https://github.com/fsbondtec/curvepress-rust/actions/workflows/release-npm.yml)
[![CI](https://github.com/fsbondtec/curvepress-rust/actions/workflows/ci.yml/badge.svg)](https://github.com/fsbondtec/curvepress-rust/actions/workflows/ci.yml)
[![CodeQL](https://github.com/fsbondtec/curvepress-rust/actions/workflows/github-code-scanning/codeql/badge.svg)](https://github.com/fsbondtec/curvepress-rust/actions/workflows/github-code-scanning/codeql)
![GitHub License](https://img.shields.io/github/license/fsbondtec/curvepress-rust)
![GitHub Release](https://img.shields.io/github/v/release/fsbondtec/curvepress-rust)



# curvepress (Rust + WASM)

Lossy time series compression -- RDP/VW point reduction + epsilon-derived quantization + varint packing.
Designed for sharp transient signals (fracture curves, impulse tests, load cells).

> **C++ / Python / Conan** bindings live in the companion repository
> [fsbondtec/curvepress](https://github.com/fsbondtec/curvepress).

## Architecture

```
raw (int64 timestamps_ns + float64 values)
  -> point reduction   (RDP / VW / RDP-N)
  -> quantization      (float64 -> uintN, bit-width from epsilon)
  -> integer packing   (delta + zigzag + LEB128 varint)
  -> byte stream
```

**No entropy-coding stage, no external compression dependencies.**

```
                     +-------------------------+
                     |   Rust core crate       |  <- ALL logic lives here
                     |   rdp  vw  quantize     |
                     |   varint  codec         |
                     +------------+------------+
                    /                           \
             wasm-bindgen                  native crate
                    |                           |
                 (WASM)                       (Rust)
              npm package                  crates.io
```

- **Rust** -- the core crate; published to crates.io.
- **WASM** -- via `wasm-bindgen` / `wasm-pack`. Direct Rust->WASM, no C ABI.

---

## Algorithms

### RDP (Ramer-Douglas-Peucker)

Recursively removes the point with the smallest perpendicular distance to the line between
its neighbours, as long as that distance is below `epsilon`. Guarantees that every dropped
point deviates at most `epsilon` from the piecewise-linear reconstruction.

- Input: `epsilon` (maximum absolute error in the value domain)
- Output: variable number of kept points
- Complexity: O(n log n) average, O(n^2) worst case
- Use when: you need a strict error bound

### VW (Visvalingam-Whyatt)

Iteratively removes the point that forms the triangle with the smallest area with its two
neighbours. Repeats until exactly `n_out` points remain.

- Input: `n_out` (exact number of output points)
- Output: exactly `n_out` points
- Complexity: O(n log n)
- Use when: you need a fixed output size (e.g. display resolution, storage budget)
- The quantization epsilon is derived automatically from the actual max deviation of
  dropped points, so no epsilon needs to be specified

### RDP-N

Binary-searches for the smallest `epsilon` that makes RDP keep at most `n_out` points.
Combines the error-bound guarantee of RDP with a target output size.

- Input: `n_out` (target maximum), `epsilon` (upper bound for the search)
- Output: at most `n_out` points
- Complexity: O(n log n * log(epsilon_range))
- Use when: you want both an error bound AND a size cap

### Axis normalization

Timestamps are in nanoseconds; values might be Newtons or millistrain. Without normalization
the time axis completely dominates Euclidean distances. curvepress always normalizes: the
time axis is scaled to match the value range before distance computation.
`epsilon` is therefore always expressed in the **value domain**.

### Error-bound contract

```
max_error <= ~1.5 * epsilon
```

| Algo  | epsilon source                                              |
|-------|-------------------------------------------------------------|
| RDP   | user-supplied                                               |
| VW    | measured max deviation of dropped points (automatic)        |
| RDP-N | measured max deviation of dropped points (automatic)        |

The 0.5x overhead comes from quantization (float64 -> integer grid at spacing epsilon).

---

## API reference

Both language bindings expose the same six functions plus `decompress`, `interpolate`,
and `version`.

### compress_rdp

Compress with RDP. `epsilon` is the maximum absolute error in the value domain.

| Language | Signature |
|----------|-----------|
| Rust     | `compress_rdp(ts: &[i64], val: &[f64], epsilon: f64) -> Result<Vec<u8>>` |
| Rust     | `compress_rdp_stats(ts, val, epsilon) -> Result<(Vec<u8>, Stats)>` |
| WASM     | `compress_rdp(BigInt64Array, Float64Array, number) -> Uint8Array` |

### compress_vw

Compress with Visvalingam-Whyatt. `n_out` is the exact number of kept points.

| Language | Signature |
|----------|-----------|
| Rust     | `compress_vw(ts: &[i64], val: &[f64], n_out: usize) -> Result<Vec<u8>>` |
| Rust     | `compress_vw_stats(ts, val, n_out) -> Result<(Vec<u8>, Stats)>` |
| WASM     | `compress_vw(BigInt64Array, Float64Array, number) -> Uint8Array` |

### compress_rdpn

Compress with RDP-N. Keeps at most `n_out` points; `epsilon` is the search upper bound.

| Language | Signature |
|----------|-----------|
| Rust     | `compress_rdpn(ts: &[i64], val: &[f64], n_out: usize, epsilon: f64) -> Result<Vec<u8>>` |
| Rust     | `compress_rdpn_stats(ts, val, n_out, epsilon) -> Result<(Vec<u8>, Stats)>` |
| WASM     | `compress_rdpn(BigInt64Array, Float64Array, number, number) -> Uint8Array` |

### decompress

Decompress a byte stream produced by any `compress_*` function.

| Language | Signature |
|----------|-----------|
| Rust     | `decompress(data: &[u8]) -> Result<(Vec<i64>, Vec<f64>)>` |
| WASM     | `decompress(Uint8Array) -> Decoded` (`Decoded.timestamps`, `Decoded.values`, `Decoded.len`) |

### interpolate

Reconstruct the value at a single timestamp `t` by linear interpolation of the support
points. Clamps (flat extrapolation) outside the data range.

| Language | Signature |
|----------|-----------|
| Rust     | `interpolate(ts: &[i64], val: &[f64], t: i64) -> Result<f64>` |
| WASM     | `interpolate(BigInt64Array, Float64Array, t: bigint) -> number` |

### Stats

Returned by the `*_stats` variants. Contains:

| Field              | Type     | Description                                      |
|--------------------|----------|--------------------------------------------------|
| `n_input`          | usize    | Number of input points                           |
| `n_kept`           | usize    | Number of points after reduction                 |
| `bytes_raw`        | usize    | Raw size (16 bytes per point)                    |
| `bytes_compressed` | usize    | Compressed byte stream length                    |
| `ratio`            | f64      | `bytes_raw / bytes_compressed`                   |
| `max_error`        | f64      | Maximum value-domain error of dropped points     |
| `quant_bits`       | u32      | Quantization bit-width used                      |

### Error handling

All functions return `Result<_, CpError>`. WASM functions panic on error (surfaced as a JavaScript exception).

| Variant              | Cause                                                           |
|----------------------|-----------------------------------------------------------------|
| `CpError::BadInput`  | Empty arrays, non-monotonic timestamps, NaN/Inf values          |
| `CpError::Corrupt`   | Byte stream is corrupted or truncated                           |

---

## Installation

### Rust (crates.io)

```bash
cargo add curvepress
```

### JavaScript / TypeScript (npm)

```bash
npm install curvepress
```

A pre-built WebAssembly package, usable from bundlers (webpack/Vite/Rollup)
and Node.js >= 18.

---

## Quick start

### Rust

```rust
use curvepress::{compress_rdp, compress_vw, compress_rdpn, decompress, interpolate};

// RDP: strict error bound
let data = compress_rdp(&timestamps_ns, &values, 1.0)?;

// VW: exact output size
let data = compress_vw(&timestamps_ns, &values, 200)?;

// RDP-N: at most 200 points, search up to epsilon=100.0
let data = compress_rdpn(&timestamps_ns, &values, 200, 100.0)?;

// Decompress
let (ts_out, val_out) = decompress(&data)?;

// Interpolate at a single timestamp
let v = interpolate(&ts_out, &val_out, 5_000_000_000_i64)?;
```

### WASM (JavaScript/TypeScript)

```typescript
import { compress_rdp, compress_vw, compress_rdpn, decompress, interpolate } from 'curvepress';

const ts  = new BigInt64Array(n);   // fill with ns timestamps
const val = new Float64Array(n);    // fill with values

// RDP
const data = compress_rdp(ts, val, 1.0);

// VW
const data = compress_vw(ts, val, 200);

// RDP-N
const data = compress_rdpn(ts, val, 200, 100.0);

// Decompress
const dec = decompress(data);
console.log(`Kept ${dec.len} of ${n} points`);

// Interpolate
const v = interpolate(dec.timestamps, dec.values, 5_000_000_000n);
```

---

## Benchmark (fracture-curve data, 100 k points)

| Algo | Ratio | Throughput | max_error |
|------|-------|------------|-----------|
| RDP epsilon=0.5 | ~18x | ~120 MB/s | <=0.75 |
| VW n=1000       | ~23x | ~80 MB/s  | informative |

*(Run `cargo bench` on your hardware for accurate numbers.)*

---

## Building & testing

```bash
# Rust tests
cargo test

# WASM (requires wasm-pack)
wasm-pack build --target nodejs --out-dir pkg --features wasm
node tests/wasm/test_wasm.mjs
```

---

## License

MIT
