# Application Layer

This document reflects the current implementation in `include/vertex/application/*` and `src/application/*`.

## Scope

`Exchange` orchestrates:

- users and wallets,
- market registration via `MarketDispatcher`,
- order submission and settlement,
- cancel/release flow,
- trade persistence.

`Exchange` does not do matching itself. Matching lives in engine workers.

## Owned State

- `accounts_`: `unordered_map<UserId, shared_ptr<Account>>`
- `accounts_mu_`: `shared_mutex` guarding map structure
- `order_meta_store_`: sharded metadata store for resting orders
- ID generators (`UserId`, `OrderId`, `TradeId`) + dedicated mutexes
- `market_dispatcher_`
- `trade_history_` (sharded, thread-safe)

`Account` currently contains:

- `User user`
- `Wallet wallet`
- `std::mutex mu`

## Errors

Current enums:

- `UserError`: `UserNotFound`, `UserAlreadyExists`, `EmptyName`
- `WalletOperationError`: `UserNotFound`, `InsufficientFunds`, `InsufficientReserved`, `InvalidQuantity`
- `PlaceOrderError`: `MarketNotListed`, `UserNotFound`, `InsufficientFunds`, `InvalidQuantity`, `InvalidAmount`, `WorkerStopped`, `OrderIdCollision`
- `CancelOrderError`: `UserNotFound`, `OrderNotFound`, `NotOrderOwner`, `MarketNotFound`, `WorkerStopped`
- `RegisterMarketError`: `AlreadyListed`, `WorkerStopped`

## Public API

User:

- `create_user(name)`
- `get_user_name(user_id)`
- `user_exists(user_id)`

Wallet:

- `deposit`, `withdraw`, `reserve`, `release`
- `free_balance`, `reserved_balance`

Trading:

- `register_market(market)`
- `place_limit_order(user_id, market, side, price, quantity)`
- `execute_market_order(user_id, market, side, order_quantity)`
- `cancel_order(user_id, order_id)`

## OrderMetaStore / TradeHistory

### OrderMetaStore

Current `OrderMeta` fields:

- `owner`
- `market`
- `side`
- `price`

Current API:

- `try_insert(order_id, meta)`
- `find(order_id)`
- `erase(order_id)`

Implementation uses 64 shards (`array<Shard, 64>`) with per-shard mutex.

### TradeHistory

Trade storage is also sharded (64 shards) by `Market` hash.

API:

- `add(Trade)`
- `market_history(market)`

## Limit Order Flow (`place_limit_order`)

Current flow:

1. Validate input (`user`, market listed, `price > 0`, `quantity > 0`).
2. Resolve taker account pointer under `accounts_mu_`.
3. Reserve taker funds (`quote = price*quantity` for BUY, `base = quantity` for SELL).
4. Generate `order_id`.
5. Insert order metadata in `order_meta_store_` before submit.
6. Submit `LimitOrderRequest` to dispatcher and wait on `future.get()`.
7. On submit error: rollback reservation and erase metadata.
8. For each `Execution`: lock buyer/seller accounts in deterministic `UserId` order, settle wallets, append trade, erase fully filled resting order metadata.
9. Return `OrderPlacementResult`.

`OrderPlacementResult` for limit orders is base-quantity based (`filled_quantity`, `remaining_quantity` in base units).

## Market Order Flow (`execute_market_order`)

Current flow:

1. Validate input.
2. Reserve taker funds (`quote budget` for BUY, `base quantity` for SELL).
3. Dispatch to helper:
   - `execute_market_buy_by_quote`
   - `execute_market_sell_by_base`
4. Helper builds request, submits via dispatcher, waits for executions.
5. Settle each execution against resting counterpart owner from `order_meta_store_`.
6. Erase fully filled resting order metadata.
7. Release unused taker reservation.
8. On submit failure: rollback full initial reservation.

Notes:

- market taker request itself is not inserted into `order_meta_store_`,
- canceling market-order result id returns `OrderNotFound`,
- market BUY path reports filled/remaining in quote units, market SELL path in base units.

## Cancel Flow (`cancel_order`)

Current flow:

1. Validate caller user existence.
2. Lookup order metadata in `order_meta_store_`.
3. Validate ownership.
4. Call `market_dispatcher_.cancel(order.market, order_id).get()`.
5. If found in book: release exact remaining reservation:
   - BUY: `remaining_quantity * price` quote
   - SELL: `remaining_quantity` base
6. Erase metadata entry.
7. Return `CancelOrderResult`.

## Register Market

`register_market` delegates to dispatcher and maps engine async errors to:

- `AlreadyListed`
- `WorkerStopped`
