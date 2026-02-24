# Engine Layer

This document reflects the current implementation in `include/vertex/engine/*` and `src/engine/*`.

## Scope

Engine layer is responsible for deterministic order matching and routing by market.

Components:

- `OrderBook` (single market)
- `MatchingEngine` (multiple markets)

## OrderBook

`OrderBook` is bound to exactly one `Market` (`market_`).

Internal structures:

- `bids_`: `std::map<Price, PriceLevel, std::greater<>>`
- `asks_`: `std::map<Price, PriceLevel, std::less<>>`
- `index_`: `std::unordered_map<OrderId, OrderLocation>`

`PriceLevel` stores FIFO list of orders:

- `std::list<std::unique_ptr<Order>> orders`

### Execution model

Current `Execution` struct:

- `OrderId buy_order_id`
- `OrderId sell_order_id`
- `Quantity quantity`
- `Price execution_price`
- `Price buy_order_limit_price`
- `bool buy_fully_filled`
- `bool sell_fully_filled`

Behavior:

- BUY order matches lowest asks while `ask_price <= buy_limit`
- SELL order matches highest bids while `bid_price >= sell_limit`
- per match: execute `min(incoming_remaining, resting_remaining)`
- filled resting orders are removed from level + index
- empty price levels are removed
- remaining incoming quantity is inserted into correct side book

### Cancel

`cancel(OrderId)` returns `std::optional<CancelResult>` with:

- `id`
- `side`
- `price`
- `remaining_quantity`

Returns `nullopt` when order is not found.

### Best prices

- `best_bid()` -> highest bid or `nullopt`
- `best_ask()` -> lowest ask or `nullopt`

## MatchingEngine

State:

- `std::unordered_map<Market, OrderBook> books_`

API:

- `std::vector<Execution> add_limit_order(std::unique_ptr<Order> order)`
- `std::optional<CancelResult> cancel(const Market&, OrderId)`
- `std::optional<Price> best_bid(const Market&) const`
- `std::optional<Price> best_ask(const Market&) const`
- `void register_market(const Market&)`
- `bool has_market(const Market&) const noexcept`

Behavior:

- `add_limit_order`, `cancel`, `best_bid`, and `best_ask` assert that market exists in `books_`
- markets must be registered before use
- `best_bid`/`best_ask` delegate to the selected `OrderBook` and return `nullopt` when that book side is empty
