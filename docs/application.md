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
- `trade_history_`

## Errors

Current enums:

- `UserError`: `UserNotFound`, `UserAlreadyExists`, `EmptyName`
- `WalletOperationError`: `UserNotFound`, `InsufficientFunds`, `InsufficientReserved`, `InvalidAmount`
- `PlaceOrderError`: `MarketNotListed`, `UserNotFound`, `WalletNotFound`, `InsufficientFunds`, `InvalidQuantity`, `InvalidAmount`
- `CancelOrderError`: `UserNotFound`, `OrderNotFound`, `NotOrderOwner`
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
- `execute_market_order(user_id, market, side, order_quantity)`
- `cancel_order(user_id, order_id)`

## Current order placement flow

`place_limit_order` currently performs:

1. input checks (`user_id`, market presence, `quantity`, user wallet presence, `price`)
2. reserve funds
3. create `LimitOrder`
4. store ownership/index mappings (`orders_`, `orders_market_`)
5. route order to `matching_engine_.add_limit_order`
6. for each `Execution`, settle wallets:
- buyer consumes quote reserved
- buyer receives quote refund when `buy_limit > execution_price`
- buyer receives base asset
- seller consumes base reserved
- seller receives quote asset
7. update `OrderPlacementResult` filled/remaining
8. create `Trade` object and persist it in `TradeHistory`
9. remove fully filled orders from ownership maps

## Current market order flow

`execute_market_order` currently performs:

1. input checks (`user_id`, market presence, `order_quantity`, user wallet presence)
2. reserve taker funds:
- BUY: reserve `market.quote()` using `order_quantity` as quote budget
- SELL: reserve `market.base()` using `order_quantity` as base quantity to sell
3. create `MarketOrder` and route it to `matching_engine_.execute_market_order`
4. for each `Execution`, settle wallets:
- buyer consumes quote reserved (`execution.quantity * execution.execution_price`)
- buyer receives base asset (`execution.quantity`)
- seller consumes base reserved (`execution.quantity`)
- seller receives quote asset (`execution.quantity * execution.execution_price`)
5. update `OrderPlacementResult`
6. persist `Trade` entries in `TradeHistory`
7. cleanup fully filled resting limit orders from ownership maps
8. release taker remainder reservation (if any)

Notes:

- Market orders are not inserted into `orders_` / `orders_market_` (they are taker-only execution requests).
- Because they are not persisted as open orders, `cancel_order` for a returned market-order id will return `OrderNotFound`.
- `OrderPlacementResult` units for `execute_market_order` are currently side-dependent:
- BUY: `filled_quantity` and `remaining_quantity` are quote amounts (spent / budget left)
- SELL: `filled_quantity` and `remaining_quantity` are base amounts (sold / unsold)
- This differs from `place_limit_order`, where filled/remaining are base quantities.

## Current cancel flow

`cancel_order` currently performs:

1. validate user existence
2. validate order existence and ownership (`orders_`)
3. resolve market from `orders_market_`
4. cancel order in matching engine
5. release reserved funds:
- BUY: release quote `remaining_quantity * price`
- SELL: release base `remaining_quantity`
6. remove order ownership/index mappings
7. return `CancelOrderResult` (`id`, `side`, `remaining_quantity`)

## Known Gaps / Current Risks

- `register_market` does not currently validate `InvalidMarket`; only duplicate check (`AlreadyListed`) is implemented.
- `WalletNotFound` in `PlaceOrderError` is currently unused (wallet lookup maps to `UserNotFound` in current implementation).
