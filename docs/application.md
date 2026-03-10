# Application Layer

This document reflects the current implementation in `include/vertex/application/*`, `src/application/*`, and `src/application/exchange/*`.

## Scope

`Exchange` orchestrates:

- users and wallets,
- market registration via `MarketDispatcher`,
- order submission and wallet settlement,
- cancel/release flow,
- market-scoped trade persistence,
- completed-order persistence (`OrderHistory`),
- pure analytics over completed orders (`order_analytics`).

`Exchange` does not do matching itself. Matching stays in engine workers.

## Owned State

- `accounts_`: `unordered_map<UserId, shared_ptr<Account>>`
- `accounts_mu_`: `shared_mutex` guarding map structure
- `order_meta_store_`: sharded metadata for open limit orders
- ID generators (`UserId`, `OrderId`, `TradeId`) + dedicated mutexes
- `market_dispatcher_`
- `trade_history_` (sharded, thread-safe, by market)
- `order_history_` (sharded, thread-safe, by order/user)

`Account` contains:

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

## Storage Components

### OrderMetaStore (open limit orders only)

Current `OrderMeta` fields:

- `owner`
- `market`
- `side`
- `price`
- `requested_base_qty`
- `executed_base_qty`
- `executed_quote_qty`
- `fill_count`
- `trade_ids`

Current API:

- `try_insert(order_id, meta)`
- `find(order_id)`
- `append_fill(order_id, trade_id, qty, price)`
- `close_and_extract(order_id, status)` -> `OrderRecord` + erase
- `erase(order_id)`

Implementation uses 64 shards (`array<Shard, 64>`) with per-shard mutex.

### OrderHistory (completed orders)

`OrderRecord` stores:

- identity/context: `id`, `user_id`, `market`, `side`, `type`, `status`
- request data: `limit_price?`, `requested_base_qty?`, `requested_quote_budget?`
- execution aggregates: `executed_base_qty`, `executed_quote_qty`, `avg_price?`, `fill_count`
- linkage: `trade_ids`

`OrderStatus` values:

- `Filled`
- `PartiallyFilled`
- `Unfilled`
- `Canceled`

API:

- `try_insert(OrderRecord)`
- `find(order_id)`
- `find_by_user(user_id)`

Implementation is sharded (64 by id + 64 by user).

### TradeHistory

Trade storage is sharded (64 shards) by `Market` hash.

API:

- `add(Trade)`
- `market_history(market)`

### Order Analytics

`order_analytics` is a pure-function module over `std::span<const OrderRecord>`.

Current API:

- `count_by_status(...)`
- `count_by_side(...)`
- `total_executed_base(...)`
- `total_executed_quote(...)`
- `average_fill_count(...)`
- `completion_ratio(...)`
- `avg_order_notional(...)`
- `vwap_from_orders(...)`
- `median_order_notional(...)`
- `top_n_by_executed_quote(...)`
- `executed_quote_by_market(...)`
- `avg_slippage_bps_for_limits(...)`
- `rank_markets_by_volume(...)`

Integration pattern:

- query completed records from `OrderHistory` (for example `find_by_user(user_id)`),
- pass returned `std::vector<OrderRecord>` as `std::span<const OrderRecord>` to analytics functions.

## Limit Order Flow (`place_limit_order`)

1. Validate input (`user`, market listed, `price > 0`, `quantity > 0`).
2. Resolve account pointer under `accounts_mu_`.
3. Reserve funds (`quote = price * quantity` for buy, `base = quantity` for sell).
4. Generate `order_id`.
5. Insert metadata into `order_meta_store_` before submit.
6. Submit `LimitOrderRequest` to dispatcher and wait on `future.get()`.
7. On submit error: rollback reservation and erase just-created metadata.
8. For each `Execution`:
   - resolve buyer/seller users from `order_meta_store_`,
   - lock both accounts in deterministic `UserId` order,
   - settle wallets,
   - create `Trade` and append to `trade_history_`,
   - call `order_meta_store_.append_fill(...)` for both order ids,
   - if an order is fully filled: `close_and_extract(..., Filled)` and insert record into `order_history_`.
9. Return `OrderPlacementResult`.

`OrderPlacementResult` for limit flow uses base units (`filled_quantity`, `remaining_quantity`).

## Market Order Flow (`execute_market_order`)

1. Validate input.
2. Reserve taker funds (`quote budget` for buy, `base quantity` for sell).
3. Dispatch to helper:
   - `execute_market_buy_by_quote`
   - `execute_market_sell_by_base`
4. Helper submits request and waits for executions.
5. For each execution:
   - settle taker/counterparty wallets,
   - create and persist `Trade`,
   - update resting counterparty in `order_meta_store_` via `append_fill`,
   - if counterparty fully filled: `close_and_extract(..., Filled)` -> `order_history_`.
6. Build taker `OrderRecord` aggregates (`executed_*`, `fill_count`, `trade_ids`, `avg_price`).
7. Set taker status:
   - `Unfilled` when `filled_quantity == 0`,
   - `PartiallyFilled` when partially executed and remainder released,
   - `Filled` when fully executed.
8. Release unused taker reservation (if any remainder exists).
9. Insert taker record into `order_history_`.
10. On submit failure: rollback full initial reservation.

Notes:

- market taker order is not inserted into `order_meta_store_`,
- canceling market-order id returns `OrderNotFound`,
- market buy reports filled/remaining in quote units, market sell in base units.

## Cancel Flow (`cancel_order`)

1. Validate caller user existence.
2. Lookup order metadata in `order_meta_store_`.
3. Validate ownership.
4. Call `market_dispatcher_.cancel(order.market, order_id).get()`.
5. If found in book: release exact remaining reservation:
   - buy: `remaining_quantity * price` quote
   - sell: `remaining_quantity` base
6. Move closed order to history: `close_and_extract(order_id, Canceled)` and insert into `order_history_`.
7. Return `CancelOrderResult`.

## Register Market

`register_market` delegates to dispatcher and maps async errors to:

- `AlreadyListed`
- `WorkerStopped`
