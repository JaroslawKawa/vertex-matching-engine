# Domain Layer

This document reflects the current implementation in `include/vertex/domain/*` and `src/domain/*`.

## Scope

Domain layer models business entities and local invariants:

- `User`
- `Wallet`
- `Order` / `LimitOrder`
- `Trade`

No infrastructure logic is implemented here.

## User

State:

- `UserId id_`
- `std::string name_`

API:

- `User(UserId id, std::string name)`
- `UserId id() const`
- `const std::string& name() const`
- defaulted comparison (`operator<=>`)

## Wallet

State:

- `std::unordered_map<Asset, Balance> balances_`

`Balance`:

- `Quantity free`
- `Quantity reserved`

Errors (`WalletError`):

- `InsufficientFunds`
- `InsufficientReserved`
- `InvalidAmount`

Mutating API (`std::expected<void, WalletError>`):

- `deposit(asset, amount)`
- `withdraw(asset, amount)`
- `reserve(asset, amount)`
- `release(asset, amount)`
- `consume_reserved(asset, amount)`

Read API:

- `Quantity free_balance(asset) const`
- `Quantity reserved_balance(asset) const`

Behavior notes:

- missing asset in read methods returns `0`
- missing asset in withdraw/reserve -> `InsufficientFunds`
- missing asset in release/consume_reserved -> `InsufficientReserved`

## Order (abstract)

State:

- `OrderId order_id_`
- `UserId user_id_`
- `Market market_`
- `Side side_`
- `Quantity initial_quantity_`
- `Quantity remaining_quantity_`

API:

- `OrderId id() const noexcept`
- `UserId user_id() const noexcept`
- `const Market& market() const noexcept`
- `Side side() const noexcept`
- `Quantity initial_quantity() const noexcept`
- `Quantity remaining_quantity() const noexcept`
- `bool is_filled() const noexcept`
- `bool is_active() const noexcept`
- `void reduce(Quantity executed)`
- `virtual Price price() const = 0`

Invariants are guarded with `assert`.

## LimitOrder

Extends `Order` with immutable `price_`.

API:

- `LimitOrder(..., Price price)`
- `Price price() const noexcept override`

Invariant:

- `price > 0` (`assert`)

## Trade

Current trade model includes both users and orders:

- `TradeId trade_id_`
- `UserId buy_user_id_`
- `UserId sell_user_id_`
- `OrderId buy_order_id_`
- `OrderId sell_order_id_`
- `Market market_`
- `Quantity quantity_`
- `Price price_`

API is immutable getters only.

Constructor invariants are enforced with `assert` (`ids valid`, `buy/sell orders different`, `quantity > 0`, `price > 0`).