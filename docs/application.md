# Application Layer

This document reflects the current implementation in `include/vertex/application/exchange.hpp` and `src/application/exchange.cpp`.

## Scope

`Exchange` orchestrates:

- users and wallets,
- market registration,
- request submission to `MatchingEngine`,
- settlement and trade recording,
- cancel and reservation cleanup.

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

Note: some enum values are currently not used by implementation paths (`WalletNotFound`, `InvalidMarket`).

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

## Limit order flow

`place_limit_order` currently performs:

1. validation (`user_id`, market, `quantity`, `price`, wallet presence),
2. reservation:
- BUY reserves quote `price * quantity`,
- SELL reserves base `quantity`,
3. build `LimitOrderRequest` and submit to `matching_engine_.submit(...)`,
4. persist ownership mapping in `orders_` and `orders_market_`,
5. settle each `Execution`:
- buyer consumes reserved quote,
- buyer receives quote refund on price improvement,
- buyer receives base,
- seller consumes reserved base,
- seller receives quote,
6. write `Trade` to `TradeHistory`,
7. remove fully filled resting orders from ownership maps,
8. return `OrderPlacementResult`.

`OrderPlacementResult` for limit orders is in base quantity units.

## Market order flow

`execute_market_order` currently:

1. validates input,
2. reserves taker funds:
- BUY reserves quote budget (`order_quantity`),
- SELL reserves base quantity (`order_quantity`),
3. dispatches to:
- `execute_market_buy_by_quote(...)`, or
- `execute_market_sell_by_base(...)`,
4. both helpers build engine request (`MarketBuyByQuoteRequest` / `MarketSellByBaseRequest`) and call `matching_engine_.submit(...)`,
5. settles executions and appends `Trade` records,
6. removes fully filled resting counterpart orders from ownership maps,
7. releases taker remainder reservation (if any).

Notes:

- market requests are taker-only and are not persisted in `orders_` / `orders_market_`,
- canceling such returned order id later will return `OrderNotFound`,
- `OrderPlacementResult` units are side-dependent:
- BUY path tracks quote spent/remaining,
- SELL path tracks base sold/remaining.

## Cancel flow

`cancel_order` currently performs:

1. validate user and ownership,
2. resolve order market,
3. call `matching_engine_.cancel(...)`,
4. release reservation:
- BUY releases quote `remaining_quantity * price`,
- SELL releases base `remaining_quantity`,
5. remove ownership maps entry,
6. return `CancelOrderResult`.

## Register market flow

`register_market` currently:

- returns `AlreadyListed` when market already exists,
- otherwise registers market in matching engine and returns success.
