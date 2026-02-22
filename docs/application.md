# Application Layer

This document describes the application-level orchestration components
of the vertex-matching-engine.

The Application layer coordinates domain objects and exposes
system use-cases.

It contains no matching logic and no infrastructure concerns.

---

# Exchange

## Responsibility

Exchange is the application-level service responsible for:

- Managing user lifecycle
- Owning User and Wallet aggregates
- Validating user existence
- Delegating financial operations to Wallet
- Mapping domain errors to application-level errors
- Enforcing system invariants

Exchange does NOT:

- Implement matching logic
- Expose Wallet directly
- Perform infrastructure operations
- Persist data

---

# Ownership Model

Exchange owns:

- `std::unordered_map<UserId, User>`
- `std::unordered_map<UserId, Wallet>`
- `IdGenerator<UserId>`

Invariant:

For every existing `UserId`, there exists exactly one `Wallet`.

No Wallet exists without a corresponding User.

---

# Error Model

Exchange exposes `ExchangeError`.

ExchangeError represents business-level failures.

Domain errors (`WalletError`) are translated into ExchangeError.

System-level invariant violations are treated as fatal
(assert + terminate).

---

# Public API

## User Management

### create_user(std::string name)

Creates a new user and associated wallet.

Constraints:

- Name must be non-empty.
- ID is generated internally.
- Strong invariant guarantees are enforced.

Returns:
- `UserId` on success
- `ExchangeError` on failure

---

## Financial Operations

Exchange delegates all balance logic to Wallet.

### deposit(UserId, Symbol, Quantity)

Increases free balance.

Possible errors:
- UserNotFound
- InvalidAmount

---

### withdraw(UserId, Symbol, Quantity)

Decreases free balance.

Possible errors:
- UserNotFound
- InvalidAmount
- InsufficientFunds

---

### reserve(UserId, Symbol, Quantity)

Moves funds from free to reserved.

Possible errors:
- UserNotFound
- InvalidAmount
- InsufficientFunds

---

### release(UserId, Symbol, Quantity)

Moves funds from reserved to free.

Possible errors:
- UserNotFound
- InvalidAmount
- InsufficientReserved

---

## Read Operations

### free_balance(UserId, Symbol)

Returns free balance.

- If symbol does not exist → returns 0
- If user does not exist → error

---

### reserved_balance(UserId, Symbol)

Returns reserved balance.

- If symbol does not exist → returns 0
- If user does not exist → error

---

### user_exists(UserId)

Returns boolean indicating existence.

---

### get_user_name(UserId)

Returns user name.

Possible errors:
- UserNotFound

---

# Architectural Notes

- Exchange is an application service, not a domain entity.
- Wallet invariants are enforced in Domain layer.
- Exchange ensures cross-aggregate consistency.
- No infrastructure dependencies exist at this layer.
- Designed to integrate with MatchingEngine in future steps.