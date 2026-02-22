# Domain Layer

This document describes domain-level components of the vertex-matching-engine.
Domain classes contain business logic but no infrastructure concerns (no threading, no I/O, no logging).

---

# User

## Responsibility

Represents the identity of a trader in the system.

User is a pure domain entity:
- It does not manage wallets.
- It does not generate IDs.
- It does not know about Exchange or MatchingEngine.

## State

- `UserId id_`
- `std::string name_`

## Properties

- Immutable after construction.
- Comparable via defaulted three-way comparison (`operator<=>`).

## API

### Constructor


User(UserId id, std::string name)


- `id` must be generated externally (Exchange responsibility).
- `name` is moved into internal storage.

### id()


UserId id() const


Returns the immutable user identifier.

### name()


const std::string& name() const


Returns user name (no copy).

---

# Wallet

## Responsibility

Represents a per-user asset ledger.

Tracks balances per symbol with separation between:
- free balance
- reserved balance

Wallet is:
- Not thread-safe (Exchange handles synchronization)
- Exception-free (uses std::expected)
- Defensive (validates all invariants)

## Invariants

For every symbol:

- `free >= 0`
- `reserved >= 0`

Operations must never violate these conditions.

## Internal State


unordered_map<Symbol, Balance> balances_;


### Balance


struct Balance {
Quantity free;
Quantity reserved;
};


## Error Handling

All mutating operations return:


std::expected<void, WalletError>


### WalletError

- `InvalidAmount`
- `InsufficientFunds`
- `InsufficientReserved`

No exceptions are thrown.

---

## Public API

### deposit(symbol, amount)

**Behavior:**
- Creates symbol entry if it does not exist.
- Increases free balance.

**Errors:**
- InvalidAmount (amount <= 0)

---

### withdraw(symbol, amount)

**Behavior:**
- Decreases free balance.

**Errors:**
- InvalidAmount
- InsufficientFunds

Symbol absence is treated as zero balance.

---

### reserve(symbol, amount)

**Behavior:**
- Moves funds from free → reserved.

**Errors:**
- InvalidAmount
- InsufficientFunds

---

### release(symbol, amount)

**Behavior:**
- Moves funds from reserved → free.

**Errors:**
- InvalidAmount
- InsufficientReserved

---

### free_balance(symbol)

Returns:
- free balance
- 0 if symbol does not exist

Does not create entries.

---

### reserved_balance(symbol)

Returns:
- reserved balance
- 0 if symbol does not exist

Does not create entries.

---

# Architectural Notes

- Wallet does not know about UserId.
- Exchange is responsible for mapping UserId → Wallet.
- Wallet does not manage symbol registry (future responsibility of Exchange).
- Overflow handling is currently not implemented (MVP assumption).

---

# Next Domain Components

- Order
- LimitOrder
- Trade