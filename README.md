# Vertex Matching Engine

![CI](https://github.com/JaroslawKawa/vertex-matching-engine/actions/workflows/ci.yml/badge.svg)

C++23 matching engine with layered architecture (`core`, `domain`, `engine`, `application`) and interactive CLI (`app/cli/*`), covered by unit and concurrency tests.

## About

This project is a C++23 matching engine designed to demonstrate production-style engineering fundamentals for exchange-like systems:

- deterministic matching behavior with explicit invariants,
- layered architecture (`core` -> `domain` -> `engine` -> `application` -> `cli`),
- strongly typed domain model (`StrongId`, `Asset`, `Market`) to reduce category errors,
- reservation-safe settlement and cancel/release flows,
- explicit error modeling with `std::expected` and typed error enums,
- automated verification with GoogleTest/CTest and CI (currently 107 tests in CTest).

## Key Features

- price-time priority order matching (FIFO within each price level),
- limit and market order flows (buy-by-quote and sell-by-base),
- partial fills, multi-level liquidity sweeps, and price improvement handling,
- reservation-aware wallet accounting (reserve/release/consume) and safe cancel flow,
- market-scoped trade persistence and multi-market routing,
- interactive CLI pipeline (`tokenizer` -> `parser` -> `dispatcher` -> `printer`) with typed parse/app errors,
- broad unit-test coverage across all layers (`core`, `domain`, `engine`, `application`, `cli`).

## Tech Stack

- C++23
- CMake
- GoogleTest
- GitHub Actions (CI)
- ThreadSanitizer (Linux target/job)

## Quick Start

```bash
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Run CLI app:

```bash
cmake --build build --config Debug --target vertex_app
./build/vertex_app            # single-config generators
./build/Debug/vertex_app.exe  # typical Windows multi-config generators
```

Run benchmark app:

```bash
cmake --build build --config Debug --target vertex_bench
./build/vertex_bench                      # single-config generators
./build/Debug/vertex_bench.exe            # typical Windows multi-config generators
./build/Debug/vertex_bench.exe --help     # benchmark CLI options
```

## Testing

- Test executable `vertex_tests` is built from `tests/CMakeLists.txt`.
- Test cases are generated into CTest automatically by `gtest_discover_tests(vertex_tests)`.
- Linux TSAN target is available with:

```bash
cmake -S . -B build-tsan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_COMPILER=clang++ -DVERTEX_ENABLE_TSAN=ON
cmake --build build-tsan --target tsan
```

- The test suite in this repository was built iteratively (AI-assisted) and then manually validated/refined.

## Architecture

- `core` - basic types, strong IDs, ID generator, asset/market primitives
- `domain` - `User`, `Wallet`, `Trade`
- `engine` - `OrderRequest`, `RestingOrder`, `OrderBook`, `MarketWorker`, `MarketDispatcher`
- `application` - `Exchange`, wallet orchestration, settlement, trade history
- `app/cli` - command AST, tokenizer, parser, dispatcher (`CliApp`), and output printer

Detailed docs are available in:

- `docs/core.md`
- `docs/domain.md`
- `docs/engine.md`
- `docs/application.md`
- `docs/cli.md`
- `docs/benchmarks.md`
- `docs/adr/0001-concurrency-model-and-invariants.md`
- `docs/concurrency-test-matrix.md`

## Documentation Note

Documentation in `docs/` is generated with AI assistance and kept aligned with the current implementation.
