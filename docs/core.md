# Core Layer

This document reflects the current implementation in `include/vertex/core/*`.

## Scope

Core layer provides foundational types and utilities:

- Strong typed IDs (`StrongId<Tag>`)
- Atomic ID generation (`IdGenerator<T>`)
- Asset and market primitives (`StrongAsset<Tag>`, `Market`)
- Common aliases (`types.hpp`)

Core layer contains no matching logic, no wallet logic, and no application orchestration.

## StrongId<Tag>

Defined in `strong_id.hpp`.

Current API:

- `constexpr StrongId() noexcept = default`
- `constexpr StrongId(std::uint64_t) noexcept`
- `constexpr bool is_valid() const noexcept`
- `constexpr std::uint64_t get_value() const noexcept`
- defaulted three-way comparison (`operator<=>`)

Properties:

- wraps `std::uint64_t`
- default value `0` means invalid
- hash specialization exists for unordered containers

## IdGenerator<T>

Defined in `id_generator.hpp`.

Current behavior:

- template-constrained to `StrongId<Tag>` via trait + `static_assert`
- internal counter: `std::atomic<std::uint64_t> counter{0}`
- `next()` returns IDs starting from `1`
- uses `fetch_add(..., memory_order_relaxed)`

## StrongAsset<Tag> / Asset

Defined in `asset.hpp` and aliased in `types.hpp`.

Current API:

- `explicit StrongAsset(std::string name)`
- `const std::string& value() const noexcept`
- defaulted three-way comparison
- hash specialization

Behavior:

- input symbol is uppercased in constructor
- non-empty name enforced by `assert`

## Market

Defined in `market.hpp`.

Current model:

- `Market(Asset base, Asset quote)`
- `const Asset& base() const noexcept`
- `const Asset& quote() const noexcept`
- defaulted three-way comparison
- hash specialization

Invariant:

- `base != quote` (enforced by `assert`)

## Common Types (`types.hpp`)

Currently defined aliases:

- `UserId`, `OrderId`, `TradeId`
- `Asset`
- `Price` (`std::int64_t`)
- `Quantity` (`std::int64_t`)
- `Side` (`Buy`, `Sell`)

Notes:

- `Market` type comes from `market.hpp`
- `AssetTag` exists in both `types.hpp` and `market.hpp` include graph; functionality works, but this is a detail worth keeping consistent during refactors.