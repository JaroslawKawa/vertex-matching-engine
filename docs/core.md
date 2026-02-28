# Core Layer

This document reflects the current implementation in `include/vertex/core/*`.

## Scope

Core layer provides foundational types and utilities:

- strong typed IDs (`StrongId<Tag>`),
- atomic ID generation (`IdGenerator<T>`),
- asset and market primitives (`StrongAsset<Tag>`, `Market`),
- common aliases (`types.hpp`).

Core layer contains no matching, wallet, or application orchestration logic.

## StrongId<Tag>

Defined in `strong_id.hpp`.

Current API:

- `constexpr StrongId() noexcept = default`
- `constexpr StrongId(std::uint64_t) noexcept`
- `constexpr bool is_valid() const noexcept`
- `constexpr std::uint64_t get_value() const noexcept`
- defaulted `operator<=>`

Properties:

- wraps `std::uint64_t`,
- `0` is treated as invalid,
- `std::hash` specialization exists for unordered containers.

## IdGenerator<T>

Defined in `id_generator.hpp`.

Current behavior:

- constrained to `StrongId<Tag>` by trait + `static_assert`,
- internal counter: `std::atomic<std::uint64_t> counter{0}`,
- `next()` returns IDs starting from `1`,
- uses `fetch_add(..., memory_order_relaxed)`.

## StrongAsset<Tag> / Asset

Defined in `asset.hpp`, with `Asset` alias in `types.hpp`.

Current API:

- `explicit StrongAsset(std::string name)`
- `const std::string& value() const noexcept`
- defaulted `operator<=>`
- `std::hash` specialization

Behavior:

- symbol is normalized to uppercase in constructor,
- non-empty input is required (`assert`).

## Market

Defined in `market.hpp`.

Current model:

- `Market(Asset base, Asset quote)`
- `const Asset& base() const noexcept`
- `const Asset& quote() const noexcept`
- defaulted `operator<=>`
- `std::hash` specialization

Invariant:

- `base != quote` (`assert`).

## Common Types (`types.hpp`)

Current aliases and enums:

- IDs: `UserId`, `OrderId`, `TradeId`
- value objects: `Asset`, `Market`
- numeric aliases: `Price` (`std::int64_t`), `Quantity` (`std::int64_t`)
- side enum: `Side::{Buy, Sell}`

Note:

- canonical `AssetTag` definition is in `types.hpp`.
- `market.hpp` only forward declares `AssetTag` to reuse the same asset strong type.
