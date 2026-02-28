# Vertex Matching Engine

![CI](https://github.com/JaroslawKawa/vertex-matching-engine/actions/workflows/ci.yml/badge.svg)

C++23 matching engine with layered architecture (`core`, `domain`, `engine`, `application`) and full unit-test suite.

## About

This project demonstrates:

- strongly typed IDs and core value objects (`Asset`, `Market`),
- deterministic order-book matching,
- clear separation between matching mechanics and application settlement,
- cancel flow and reservation-aware wallet settlement,
- multi-market routing in a single matching engine.

## Key Features

- limit order requests with price-time priority (FIFO in one price level),
- market buy by quote-budget and market sell by base-quantity,
- partial fills, multi-level sweeps, and price improvement,
- order cancel with reservation release,
- trade persistence in market-scoped trade history.

## Tech Stack

- C++23
- CMake
- GoogleTest
- GitHub Actions (CI)

## Quick Start

```bash
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## Testing

- Test executable `vertex_tests` is built from `tests/CMakeLists.txt`.
- Test cases are generated into CTest automatically by `gtest_discover_tests(vertex_tests)`.
- The test suite in this repository was built iteratively (AI-assisted) and then manually validated/refined.

## Architecture

- `core` - basic types, strong IDs, ID generator, asset/market primitives
- `domain` - `User`, `Wallet`, `Trade`
- `engine` - `OrderRequest`, `RestingOrder`, `OrderBook`, `MatchingEngine`
- `application` - `Exchange`, wallet orchestration, settlement, trade history

Detailed docs are available in:

- `docs/core.md`
- `docs/domain.md`
- `docs/engine.md`
- `docs/application.md`

## Documentation Note

Documentation in `docs/` is generated with AI assistance and kept aligned with the current implementation.
