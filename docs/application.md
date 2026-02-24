# Application Layer

This document reflects the current implementation in `include/vertex/application/exchange.hpp` and `src/application/exchange.cpp`.

## Scope

`Exchange` orchestrates users, wallets, market registration, order placement, and settlement effects from engine executions.

Owned state:

- `users_`: `unordered_map<UserId, User>`
- `wallets_`: `unordered_map<UserId, Wallet>`
- `orders_`: `unordered_map<OrderId, UserId>`
- `orders_market_`: `unordered_map<OrderId, Market>`
- generators: `UserIdGenerator`, `OrderIdGenerator`, `TradeIdGenerator`
- `matching_engine_`

## Errors

Current enums:

- `UserError`: `UserNotFound`, `UserAlreadyExists`, `EmptyName`
- `WalletOperationError`: `UserNotFound`, `InsufficientFunds`, `InsufficientReserved`, `InvalidAmount`
- `PlaceOrderError`: `MarketNotListed`, `UserNotFound`, `WalletNotFound`, `InsufficientFunds`, `InvalidQuantity`, `InvalidAmount`
- `RegisterMarketError`: `AlreadyListed`, `InvalidMarket`

Note: some enum values are currently unused (`WalletNotFound`, `InvalidMarket`).

## Public API

User:

- `create_user(name)`
- `get_user_name(user_id)`
- `user_exists(user_id)`

Wallet passthrough:

- `deposit`, `withdraw`, `reserve`, `release`
- `free_balance`, `reserved_balance`

Market and trading:

- `register_market(market)`
- `place_limit_order(user_id, market, side, price, quantity)`

## Current order placement flow

`place_limit_order` currently performs:

1. input checks (`user_id`, market presence, `quantity`, user wallet presence, `price`)
2. reserve funds
3. create `LimitOrder`
4. store ownership/index mappings (`orders_`, `orders_market_`)
5. route order to `matching_engine_.add_order`
6. for each `Execution`, settle wallets:
- buyer consumes quote reserved
- buyer receives quote refund when `buy_limit > execution_price`
- buyer receives base asset
- seller consumes base reserved
- seller receives quote asset
7. update `OrderPlacementResult` filled/remaining
8. create `Trade` object (currently not persisted)
9. remove fully filled orders from ownership maps

## Known Gaps / Current Risks

- In current code, market-not-listed check in `place_limit_order` is inverted (`if (matching_engine_.has_market(market)) return MarketNotListed;`). This should be negated to match intended behavior.
- `register_market` does not currently validate `InvalidMarket`; only duplicate check (`AlreadyListed`) is implemented.
- Trade history storage is not implemented yet (trade object is created and dropped).