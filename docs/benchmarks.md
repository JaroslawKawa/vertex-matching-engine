# Benchmark Scenarios

This document describes what each benchmark scenario measures in `vertex_bench`.

## Scope and Model

Benchmark model:

`many client threads -> one shared Exchange -> one shared MarketDispatcher -> one worker thread per market`

Each scenario reports:

- `Ops per sec` (successful operations per second),
- `Total ops` (successful operations counted during measurement window),
- latency percentiles: `p50`, `p95`, `p99` in microseconds.

Warmup and measurement windows come from `BenchConfig`.

## Scenario Meanings

### `SingleMarketHighLoad`

- One market only.
- All threads send operations to that one market.
- Buyers and sellers are sampled from shared pools for that market.

What it measures:

- single-market throughput ceiling,
- contention when all traffic is forced through one market worker.

### `MultiMarketParallelLoad`

- Multi-market scenario.
- Thread-to-market routing uses `tid % markets.size()`.
- In current implementation this reuses the disjoint-user logic and is labeled as `MultiMarketParallelLoad`.

What it measures:

- aggregate throughput when load is spread across multiple market workers.

### `DisjointUsersContention`

- Multi-market scenario.
- Thread-to-market routing uses `tid % markets.size()`.
- Each market has its own buyer/seller pools (no user overlap across markets).

What it measures:

- near best-case parallelism with low account-lock contention across markets.

### `SharedUsersContention`

- Multi-market scenario.
- Thread-to-market routing uses `tid % markets.size()`.
- The same global buyer/seller pools are used across all markets.
- Users are funded for all assets required by all benchmark markets.

What it measures:

- contention-heavy behavior when the same accounts are hot across markets,
- lock/contention overhead in `Exchange` state handling.

## Reading Results

- Compare `DisjointUsersContention` vs `SharedUsersContention` to estimate contention cost.
- Compare `SingleMarketHighLoad` vs multi-market scenarios to estimate scaling from per-market workers.
- Use `p95/p99` for tail-latency regressions; `Ops per sec` alone is not enough.

## Running Benchmarks

```bash
cmake --build build --target vertex_bench
./build/vertex_bench
```

Windows multi-config generators commonly use:

```bash
./build/Debug/vertex_bench.exe
```
