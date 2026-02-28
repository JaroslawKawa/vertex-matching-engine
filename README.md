# Vertex Matching Engine

![CI](https://github.com/JaroslawKawa/vertex-matching-engine/actions/workflows/ci.yml/badge.svg)

A C++23 matching engine project that supports limit and market orders, built with a layered architecture and solid unit-test coverage.

## About

This portfolio project demonstrates:

- domain-driven modeling (`Order`, `Trade`, `Wallet`, `User`),
- order book and matching logic implementation,
- clean separation between engine and application layers,
- quality assurance through tests and CI.

## Key Features

- `LIMIT` and `MARKET` order handling,
- best-price matching with FIFO inside each price level,
- order cancel flow with reserved-funds release,
- trade settlement and trade history tracking,
- multi-market support through `MatchingEngine`.

## Tech Stack

- C++23
- CMake
- GoogleTest
- GitHub Actions (CI)

## Quick Start

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Architecture

- `core` - basic types and ID generators
- `domain` - domain entities and business rules
- `engine` - order book and matching logic
- `application` - orchestration, wallets, and settlement

Detailed docs are available in:

- `docs/core.md`
- `docs/domain.md`
- `docs/engine.md`
- `docs/application.md`

## Documentation Note

Documentation in `docs/` is generated with AI assistance and kept aligned with the current implementation.
