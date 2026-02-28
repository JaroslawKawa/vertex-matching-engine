# Engine Layer

This document reflects the current implementation in `include/vertex/engine/*` and `src/engine/*`.

## Scope

Engine layer is responsible for deterministic matching and market-level routing.

Main components:

- `OrderRequest` (incoming request model)
- `RestingOrder` (booked passive order model)
- `OrderBook` (single market)
- `MatchingEngine` (multi-market router)

Engine does not manage wallet balances or user settlement.

## Request Model (`order_request.hpp`)

Incoming requests are represented by `std::variant`:

- `LimitOrderRequest`
- `MarketBuyByQuoteRequest`
- `MarketSellByBaseRequest`

### LimitOrderRequest

Fields:

- `OrderId id`
- `UserId user_id`
- `Market market`
- `Side side`
- `Price limit_price`
- `Quantity base_quantity`

### MarketBuyByQuoteRequest

Fields:

- `OrderId id`
- `UserId user_id`
- `Market market`
- `Quantity quote_budget`

### MarketSellByBaseRequest

Fields:

- `OrderId id`
- `UserId user_id`
- `Market market`
- `Quantity base_quantity`

## RestingOrder (`resting_order.hpp`)

Represents passive order already stored in the book.

Fields:

- `OrderId id`
- `Price limit_price`
- `Quantity initial_base_quantity`
- `Quantity remaining_base_quantity`

Helpers:

- `reduce(executed)`
- `is_filled()`
- `is_active()`

`reduce` enforces invariant via `assert(executed > 0 && executed <= remaining_base_quantity)`.

## OrderBook

`OrderBook` is bound to exactly one `Market`.

Internal structures:

- `bids_`: `std::map<Price, PriceLevel, std::greater<>>`
- `asks_`: `std::map<Price, PriceLevel, std::less<>>`
- `index_`: `std::unordered_map<OrderId, OrderLocation>`

`PriceLevel` stores FIFO list:

- `std::list<RestingOrder> orders`

### Public API

- `insert_resting(Side side, RestingOrder&& order)`
- `match_limit_buy_against_asks(OrderId taker_order_id, Price limit_price, Quantity& remaining_base_quantity)`
- `match_limit_sell_against_bids(OrderId taker_order_id, Price limit_price, Quantity& remaining_base_quantity)`
- `match_market_buy_by_quote_against_asks(OrderId taker_order_id, Quantity remaining_quote_budget)`
- `match_market_sell_by_base_against_bids(OrderId taker_order_id, Quantity remaining_base_quantity)`
- `cancel(OrderId)`
- `best_bid()`
- `best_ask()`

### Execution model

`Execution` fields:

- `OrderId buy_order_id`
- `OrderId sell_order_id`
- `Quantity quantity` (base quantity)
- `Price execution_price`
- `Price buy_order_limit_price`
- `bool buy_fully_filled`
- `bool sell_fully_filled`

Behavior:

- limit buy matches lowest asks while `ask <= limit_price`,
- limit sell matches highest bids while `bid >= limit_price`,
- market buy consumes asks by quote budget,
- market sell consumes bids by base quantity,
- filled resting orders are removed from price level and `index_`,
- empty price levels are erased,
- market remainder is not inserted into the book.

## MatchingEngine

State:

- `std::unordered_map<Market, OrderBook> books_`

Public API:

- `register_market(const Market&)`
- `has_market(const Market&) const noexcept`
- `submit(OrderRequest&&)`
- `cancel(const Market&, OrderId)`
- `best_bid(const Market&) const`
- `best_ask(const Market&) const`

Behavior:

- `submit(...)` uses `std::visit` and dispatches by request type,
- limit request is matched first; if remainder exists, it is converted to `RestingOrder` and inserted via `insert_resting`,
- market requests only match against current book liquidity,
- `cancel`, `best_bid`, `best_ask`, and request handlers assert that market is registered.
