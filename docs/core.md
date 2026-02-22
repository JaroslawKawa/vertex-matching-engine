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


std::atomicstd::uint64_t
 counter{0};


## Behavior

- First generated ID is 1
- Each call increments counter atomically
- Uses `memory_order_relaxed`

## Public API

### next()


T next()


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

# Architectural Notes

- Core layer contains no domain knowledge.
- StrongId prevents category errors at compile time.
- IdGenerator separates ID creation from entity construction.
- Exchange is responsible for owning ID generators.

---

# Core Types

Defined in `types.hpp`.

Includes:

- `UserId`
- `OrderId`
- `Price`
- `Quantity`
- `Symbol`
- `Side`

Core types provide foundational building blocks for higher layers.