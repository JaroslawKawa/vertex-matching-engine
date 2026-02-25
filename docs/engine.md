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

Current order-routing API in `OrderBook`:

- `std::vector<Execution> add_limit_order(std::unique_ptr<LimitOrder> order)`
- `std::vector<Execution> execute_market_order(std::unique_ptr<MarketOrder> order)`

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

- `std::vector<Execution> add_limit_order(std::unique_ptr<LimitOrder> order)`
- `std::vector<Execution> execute_market_order(std::unique_ptr<MarketOrder> order)`
- `std::optional<CancelResult> cancel(const Market&, OrderId)`
- `std::optional<Price> best_bid(const Market&) const`
- `std::optional<Price> best_ask(const Market&) const`
- `void register_market(const Market&)`
- `bool has_market(const Market&) const noexcept`

Behavior:

- `add_limit_order`, `execute_market_order`, `cancel`, `best_bid`, and `best_ask` assert that market exists in `books_`
- markets must be registered before use
- `best_bid`/`best_ask` delegate to the selected `OrderBook` and return `nullopt` when that book side is empty
- `add_limit_order` and `execute_market_order` delegate to the selected `OrderBook`

### `OrderBook::execute_market_order`

Current behavior:

- matches against opposite side until incoming order is filled or book side becomes empty
- does not insert market order remainder into the book
- removes filled resting orders from level + index
- removes empty price levels
- quantity semantics are side-dependent in current implementation:
- `BUY`: `remaining_quantity` is quote-budget remaining (amount you can still spend)
- `SELL`: `remaining_quantity` is base quantity remaining to sell
- `BUY` uses integer math (`remaining_quote / price`) so execution stops when remaining budget is below best ask price

Current contract:

- method accepts `std::unique_ptr<MarketOrder>`
- market-order remainder is not inserted into the book (remaining quantity is dropped)
