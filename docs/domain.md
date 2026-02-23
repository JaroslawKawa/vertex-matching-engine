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

Tracks balances per asset with separation between:
- free balance
- reserved balance

Wallet is:
- Not thread-safe (Exchange handles synchronization)
- Exception-free (uses std::expected)
- Defensive (validates all invariants)

## Invariants

For every asset:

- `free >= 0`
- `reserved >= 0`

Operations must never violate these conditions.

## Internal State


unordered_map<Asset, Balance> balances_;


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

### deposit(asset, amount)

**Behavior:**
- Creates asset entry if it does not exist.
- Increases free balance.

**Errors:**
- InvalidAmount (amount <= 0)

---

### withdraw(asset, amount)

**Behavior:**
- Decreases free balance.

**Errors:**
- InvalidAmount
- InsufficientFunds

Asset absence is treated as zero balance.

---

### reserve(asset, amount)

**Behavior:**
- Moves funds from free → reserved.

**Errors:**
- InvalidAmount
- InsufficientFunds

---

### release(asset, amount)

**Behavior:**
- Moves funds from reserved → free.

**Errors:**
- InvalidAmount
- InsufficientReserved

---

### free_balance(asset)

Returns:
- free balance
- 0 if asset does not exist

Does not create entries.

---

### reserved_balance(asset)

Returns:
- reserved balance
- 0 if asset does not exist

Does not create entries.

---

# Architectural Notes

- Wallet does not know about UserId.
- Exchange is responsible for mapping UserId → Wallet.
- Wallet does not manage asset registry (future responsibility of Exchange).
- Overflow handling is currently not implemented (MVP assumption).

---


# Order

## Responsibility

Represents a trading order within the system.

Order is a domain aggregate responsible for:

- Identity (`OrderId`)
- Ownership (`UserId`)
- Instrument (`Asset`)
- Direction (`Side`)
- Quantity lifecycle management

Order contains business invariants and state transition logic
but has no knowledge of:

- MatchingEngine
- Exchange
- OrderBook
- Persistence
- Infrastructure

---

## State

- `OrderId order_id_`
- `UserId user_id_`
- `Asset asset_`
- `Side side_`
- `Quantity initial_quantity_`
- `Quantity remaining_quantity_`

---

## Invariants

- `order_id` must be valid
- `user_id` must be valid
- `initial_quantity > 0`
- `0 <= remaining_quantity <= initial_quantity`

Invariant violations terminate the program via `assert`
(because they represent internal engine bugs, not business errors).

---

## Quantity Lifecycle

Orders start fully active:

remaining_quantity = initial_quantity

The only state mutation allowed is via:

reduce(executed_quantity)

Rules:

- `executed_quantity > 0`
- `executed_quantity <= remaining_quantity`

When `remaining_quantity == 0`,
the order is considered filled.

---

## Polymorphism

Order is an abstract base class.

It defines:

virtual Price price() const = 0;

Concrete order types must implement price semantics.

---

# LimitOrder

## Responsibility

Represents a limit order with a fixed price.

Extends Order by adding:

- `Price price_`

---

## Invariants

- `price > 0`

Price is immutable after construction.

---

## Behavior

Implements:

Price price() const noexcept override;

LimitOrder does not introduce additional mutable state.

---

# Architectural Notes

- Order does not generate its own ID.
- ID generation is responsibility of higher layers (MatchingEngine).
- Order enforces internal invariants via assert.
- Wallet handles business validation via std::expected.
- Domain layer never throws exceptions for control flow.

---

# Trade

## Responsibility

Represents an immutable fact of execution resulting from matching
a buy and a sell order.

Trade is a domain aggregate that:

- Links two orders (`buy_order_id`, `sell_order_id`)
- Captures execution details (`asset`, `quantity`, `price`)
- Enforces strict internal invariants
- Contains no business process logic

Trade does not:

- Modify orders
- Update wallets
- Know about users
- Perform matching
- Generate its own ID

---

## State

- `TradeId trade_id_`
- `OrderId buy_order_id_`
- `OrderId sell_order_id_`
- `Asset asset_`
- `Quantity quantity_`
- `Price price_`

All members are immutable after construction.

---

## Invariants

The constructor enforces:

- `trade_id` must be valid
- `buy_order_id` must be valid
- `sell_order_id` must be valid
- `buy_order_id != sell_order_id`
- `asset` must be non-empty
- `quantity > 0`
- `price > 0`

Invariant violations terminate the program via `assert`,
as they represent internal engine errors.

---

## Design Properties

- Immutable (value-like semantics)
- No polymorphism
- No exception-based error handling
- No infrastructure dependencies
- No ownership of external aggregates

Trade represents a completed, irreversible market event.