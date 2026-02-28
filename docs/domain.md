# Domain Layer

This document reflects the current implementation in `include/vertex/domain/*` and `src/domain/*`.

## Scope

Domain layer models business entities and local invariants:

- `User`
- `Wallet`
- `Trade`

No matching engine routing or order-book mechanics are implemented here.

## Important Architectural Note

There is currently no `Order` / `LimitOrder` / `MarketOrder` hierarchy in `domain`.
Order intent is modeled in the engine layer via `OrderRequest` variants (`engine/order_request.hpp`).

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

- missing asset in reads returns `0`,
- missing asset in `withdraw`/`reserve` returns `InsufficientFunds`,
- missing asset in `release`/`consume_reserved` returns `InsufficientReserved`.

## Trade

Current trade model:

- `TradeId trade_id_`
- `UserId buy_user_id_`
- `UserId sell_user_id_`
- `OrderId buy_order_id_`
- `OrderId sell_order_id_`
- `Market market_`
- `Quantity quantity_`
- `Price price_`

API is immutable getters only.

Constructor invariants are asserted:

- valid trade/order IDs,
- `buy_order_id != sell_order_id`,
- `quantity > 0`,
- `price > 0`.
