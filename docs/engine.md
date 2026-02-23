# Engine Layer

This document describes the matching engine components
of the vertex-matching-engine.

The Engine layer is responsible for deterministic order matching.
It contains no user logic, no wallet logic, and no application orchestration.

The Engine layer operates purely on orders.

---

# OrderBook

## Responsibility

OrderBook represents a single-instrument matching engine.

Each OrderBook instance manages exactly one `Symbol`.

It is responsible for:

- Maintaining price-time priority
- Matching incoming orders
- Producing execution results
- Managing active order storage
- Supporting order cancellation
- Providing best bid / best ask queries

OrderBook does NOT:

- Know about users
- Manage wallets
- Generate TradeId
- Create Trade objects
- Persist data
- Handle threading

---

## Ownership Model

OrderBook owns all active orders:

- `std::unique_ptr<Order>` is stored internally
- Ownership never leaves the OrderBook
- Orders are destroyed on:
  - Full execution
  - Cancellation

External layers never receive `Order` objects back.

---

## Internal Data Structures

### Price Levels

Orders are grouped by price:

- `bids_` → `std::map<Price, PriceLevel, std::greater<>>`
- `asks_` → `std::map<Price, PriceLevel, std::less<>>`

Each `PriceLevel` contains:


std::list<std::unique_ptr<Order>>


The list guarantees FIFO execution within the same price.

---

### Order Index


std::unordered_map<OrderId, OrderLocation>


`OrderLocation` stores:

- `Side`
- `Price`
- Iterator to the order in the list

This enables O(1) cancellation.

Invariant:

If an `OrderId` exists in `index_`,
then the corresponding order must exist in exactly one price level.

---

## Matching Model

Matching follows strict price-time priority.

For incoming BUY:

- Match against lowest ask
- Continue while:
  - Remaining quantity > 0
  - Best ask price ≤ order price

For incoming SELL:

- Match against highest bid
- Continue while:
  - Remaining quantity > 0
  - Best bid price ≥ order price

Within a price level:

- Orders are matched FIFO
- Only the front order is considered per iteration

---

## Execution

Matching produces:


struct Execution {
OrderId buy_order_id;
OrderId sell_order_id;
Quantity quantity;
Price price;
};


Execution is an engine-level result.

It represents a mechanical match,
not a business-level trade.

Trade objects are constructed in the Application layer.

---

## Partial and Full Fills

For each match:

- Executed quantity = min(incoming.remaining, resting.remaining)
- Both orders are reduced
- If resting becomes filled:
  - Remove from list
  - Remove from index_
- If price level becomes empty:
  - Remove price level from map

If incoming order remains active after matching,
it is inserted into its corresponding side map.

---

## Cancel

Cancel removes an active order from the book.

Signature:


std::optional<CancelResult> cancel(OrderId);


CancelResult contains:

- OrderId
- Side
- Price
- Remaining quantity

Behavior:

- If OrderId not found → return nullopt
- If found:
  - Remove from price level
  - Remove empty level if necessary
  - Remove from index_
  - Return remaining quantity information

Cancel does not transfer ownership of Order.

Order is destroyed inside OrderBook.

---

## Queries

### best_bid()

Returns:

- Highest bid price
- nullopt if no bids

### best_ask()

Returns:

- Lowest ask price
- nullopt if no asks

These are state queries and do not represent errors.

---

## Invariants

- No empty price levels exist in bids_ or asks_
- All active orders exist in index_
- No OrderId exists twice
- remaining_quantity > 0 for all active orders
- All orders belong to the OrderBook's Symbol

Invariant violations are fatal (assert).

---

# Architectural Notes

Engine layer is:

- Deterministic
- Exception-free
- Ownership-safe
- Single-threaded (synchronization handled externally)

Engine does not know:

- UserId
- Wallet
- Exchange
- TradeHistory
- Infrastructure

It is a pure matching component.