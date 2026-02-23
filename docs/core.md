# Core Layer

This document describes low-level foundational components of the vertex-matching-engine.

Core components provide:

- Strong type safety
- ID generation
- Fundamental system primitives

Core layer contains no business logic and no infrastructure logic.

---

# StrongId<Tag>

## Responsibility

Provides strongly-typed identifiers to prevent accidental mixing of domain IDs.

Examples:

- `UserId`
- `OrderId`
- `TradeId`

Prevents bugs such as assigning an OrderId to a UserId.

## Design

template<typename Tag>
class StrongId

Each Tag creates a distinct type at compile time:

StrongId<UserTag> != StrongId<OrderTag>

## Properties

- Wraps `std::uint64_t`
- Zero runtime overhead
- Default value (0) represents invalid ID
- Explicit constructor
- Defaulted three-way comparison (`operator<=>`)

## Invariants

- `value == 0` → invalid
- `value > 0` → valid

## Public API

### Constructor

explicit StrongId(std::uint64_t value)

### isValid()

bool isValid() const

Returns true if ID is non-zero.

### getValue()

std::uint64_t getValue() const

Exposes underlying numeric value.

---

# IdGenerator<T>

## Responsibility

Generates monotonically increasing, strongly-typed identifiers.

Designed to be:

- Lock-free
- Thread-safe
- Low-overhead

## Template Constraint

T must be a specialization of `StrongId<Tag>`.

Enforced via trait-based `static_assert`.

## Internal State

std::atomic<std::uint64_t> counter{0};

## Behavior

- First generated ID is 1
- Each call increments counter atomically
- Uses `memory_order_relaxed`

## Public API

### next()

T next();

Returns a new unique identifier.

## Thread Safety

- `fetch_add` ensures uniqueness
- `memory_order_relaxed` is sufficient
- No global synchronization required

## Overflow

- After `2^64 - 1`, counter wraps to 0
- Practically irrelevant for MVP
- Not currently guarded

---

# Asset

## Responsibility

Represents a single tradable instrument (e.g., BTC, USDT).

Asset is a strongly-typed wrapper around a normalized string identifier.

## Design

using Asset = StrongAsset<AssetTag>;

## Properties

- Explicit constructor from `std::string`
- Internally normalized to uppercase
- Immutable after construction
- Value type (copyable, comparable)
- Hashable
- Defaulted three-way comparison

## Invariants

- Name must not be empty
- Name is stored in uppercase form

## Purpose

Separates asset identity from trading pair identity.

Used by:

- Wallet (balances per asset)
- Market (base and quote assets)

---

# Market

## Responsibility

Represents a trading pair between two distinct assets.

Examples:

- BTC / USDT
- ETH / USDT

Market is a value-type that describes a matching context.

## Design

class Market
{
    Asset base;
    Asset quote;
}

## Properties

- Immutable after construction
- Value type
- Hashable
- Defaulted three-way comparison
- No business logic

## Invariants

- base != quote

## Purpose

- Used by MatchingEngine as key for order books
- Defines the settlement direction:
  - BUY → pay quote, receive base
  - SELL → pay base, receive quote

---

# Architectural Notes

- Core layer contains no domain knowledge.
- StrongId prevents category errors at compile time.
- IdGenerator separates ID creation from entity construction.
- Asset and Market separate instrument identity from trading context.
- Exchange is responsible for owning ID generators.

---

# Core Types

Defined in `types.hpp`.

Includes:

- `UserId`
- `OrderId`
- `TradeId`
- `Price`
- `Quantity`
- `Asset`
- `Market`
- `Side`

Core types provide foundational building blocks for higher layers.