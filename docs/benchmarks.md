# Benchmark Scenarios

This document describes benchmark coverage implemented in `bench/main.cpp` and `bench/benchmark_runner.*`.

## Runtime Model

Benchmarks stress this model:

`many client threads -> one shared Exchange -> one shared MarketDispatcher -> one worker thread per market`

Each run reports:

- `ops_per_sec`
- `total_ops`
- latency percentiles (`p50`, `p95`, `p99`) in microseconds

Warmup/measurement/repeat counts are controlled by `BenchConfig`.

## Scenarios

### `SingleMarketHighLoad`

- One market.
- All benchmark threads target that same market.

Measures:

- throughput ceiling for single market worker,
- latency under maximum same-market contention.

### `MultiMarketParallelLoad`

- Multi-market load spread by `market_idx = tid % markets.size()`.
- Current implementation reuses `run_disjoint_users(...)` logic and only changes scenario label.

Measures:

- aggregate throughput when load is spread across multiple workers.

### `DisjointUsersContention`

- Multi-market routing (`tid % markets.size()`).
- Each market has separate buyer/seller pools.

Measures:

- near best-case throughput with limited cross-market account lock contention.

### `SharedUsersContention`

- Multi-market routing (`tid % markets.size()`).
- All markets share one global buyer pool and one global seller pool.
- Users are funded for all assets required by all markets.

Measures:

- lock contention in `Exchange` account model,
- throughput drop vs disjoint users under shared hot accounts.

## Operation Mix

Per-thread operation draw (`pick_random_op`):

- 30% `PlaceLimitBuy`
- 30% `PlaceLimitSell`
- 25% `MarketBuy`
- 10% `MarketSell`
- 5% `Cancel` (implemented as place + cancel)

## CLI Options (`vertex_bench`)

- `--scenario <single|multi|disjoint|shared|all>` (also comma-separated list)
- `--threads <int>`
- `--warmup <int>`
- `--measure <int>`
- `--repeats <int>`
- `--seed <uint32>`
- `--json-out <path>`
- `--verbose` / `--quiet`
- `--help`

## JSON Output

When `--json-out` is used, benchmark writes:

- `benchmarks`: raw per-run metrics grouped by scenario
- `aggregates`: per-scenario medians (`median_ops_per_sec`, median `p50/p95/p99`)

## Running

```bash
cmake --build build --target vertex_bench
./build/vertex_bench --help
./build/vertex_bench --scenario all --repeats 5 --threads 24 --json-out bench-results.json
```

Windows multi-config:

```bash
./build/Debug/vertex_bench.exe --help
```
