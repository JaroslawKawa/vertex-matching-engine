# Vertex Matching Engine

![CI](https://github.com/JaroslawKawa/vertex-matching-engine/actions/workflows/ci.yml/badge.svg)

A **C++23 matching engine** demonstrating production-style engineering practices for exchange-like systems, including deterministic matching, layered architecture, strong domain modeling, and automated verification.

---

## Overview

This project implements a simplified **exchange-style trading engine** capable of processing limit and market orders across multiple markets.

The system demonstrates several core engineering concerns typical for trading infrastructure:

- deterministic **price-time priority matching**
- **reservation-safe wallet accounting**
- **strongly typed domain model** to prevent category errors
- **layered architecture** with strict dependency boundaries
- **explicit error modeling** with `std::expected`
- **automated verification** with unit and concurrency tests

The goal of the project is to explore how **modern C++23 can be used to design deterministic and maintainable trading systems.**

Some parts of the project were developed with **AI assistance**:

- the **system architecture** was designed collaboratively with AI
- the **entire test suite** was initially generated with AI assistance
- the **project documentation** was generated with AI assistance
- selected **CLI utilities** were generated with AI support
- the **benchmark tool** was partially AI-assisted
- **CMake configuration** and **CI workflows** were generated with AI assistance

All AI-generated components were **manually reviewed, validated, and refined** to ensure correctness and consistency with the implementation.

---

# Engineering Highlights

The project demonstrates several production-oriented design patterns:

- **Layered architecture**

```text
CLI
|
V
Application Layer (Exchange orchestration)
|
V
Matching Engine (OrderBook / MarketWorker / Dispatcher)
|
V
Domain Model (Wallet / Trade / User)
|
V
Core Types (StrongId / Asset / Market)
```

- **Deterministic price-time matching** with FIFO priority within each price level
- **Reservation-safe settlement model** preventing inconsistent wallet states
- **Strongly typed identifiers (`StrongId`)** preventing category errors
- **Explicit error modeling** using `std::expected` and typed error enums
- **Market-scoped trade persistence** and multi-market routing
- **Concurrency-safe execution model** verified with ThreadSanitizer
- **Automated CI verification** with unit and concurrency tests

---

# Key Features

- price-time priority order matching (FIFO within each price level)
- limit and market order flows (buy-by-quote and sell-by-base)
- partial fills and multi-level liquidity sweeps
- price improvement handling
- reservation-aware wallet accounting (`reserve`, `release`, `consume`)
- safe order cancel flow with fund release
- market-scoped trade persistence
- completed-order persistence with status tracking (`Filled`, `PartiallyFilled`, `Unfilled`, `Canceled`)
- order analytics helpers over `OrderRecord` sets (`count`, `vwap`, `median`, `top-n`, `slippage`, market ranking)
- multi-market routing via dispatcher
- interactive CLI pipeline:
  - tokenizer
  - parser
  - dispatcher
  - printer

---

# Tech Stack

- **C++23**
- **CMake**
- **GoogleTest**
- **GitHub Actions CI**
- **ThreadSanitizer (Linux target)**

---

# Quick Start

Build project:

```bash
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Run CLI application:

```bash
cmake --build build --config Debug --target vertex_app
./build/vertex_app            # single-config generators
./build/Debug/vertex_app.exe  # Windows multi-config generators
```

Run benchmark tool:

```bash
cmake --build build --config Debug --target vertex_bench
./build/vertex_bench
./build/Debug/vertex_bench.exe
./build/Debug/vertex_bench.exe --help
```

---

# Testing

The project includes extensive automated tests.

- Unit tests cover core, domain, engine, application, and CLI layers
- CTest integration via `gtest_discover_tests`
- ThreadSanitizer CI job verifies concurrency safety

Example TSAN run:

```bash
cmake -S . -B build-tsan -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DVERTEX_ENABLE_TSAN=ON

cmake --build build-tsan --target tsan
```

Currently the project contains 100+ automated tests executed in CI.

---

# Performance

The repository includes a synthetic benchmark tool evaluating the behavior of the matching engine under different workloads.

The benchmark evaluates:

- matching throughput
- concurrent order submission
- multi-market execution scenarios

Benchmark results and methodology are documented in:

- `docs/benchmarks.md`

---

# Architecture

Project modules:

- `core`: basic types, strong identifiers, ID generator, asset and market primitives
- `domain`: domain entities such as `User`, `Wallet`, and `Trade`
- `engine`: order requests, resting orders, order book logic, market workers and dispatching
- `application`: exchange orchestration, wallet settlement, trade history, order history, and order analytics
- `app/cli`: interactive CLI interface including tokenizer, parser, dispatcher and printer

---

# Documentation

Detailed documentation is available in:

- `docs/core.md`
- `docs/domain.md`
- `docs/engine.md`
- `docs/application.md`
- `docs/cli.md`
- `docs/benchmarks.md`
- `docs/concurrency-test-matrix.md`
- `docs/adr/0001-concurrency-model-and-invariants.md`
- `docs/adr/0002-rest-api-transport-and-boundaries.md`

Documentation is partially AI-assisted and manually reviewed to ensure consistency with the implementation.

---

# Project Goals

This project was created to demonstrate:

- modern C++23 design
- deterministic system architecture
- exchange-style matching engine design
- strong domain modeling
- CI-driven verification workflows
