# ADR-0001: Concurrency Model, Invariants, and Lock Ordering

- Status: Accepted
- Date: 2026-03-03
- Owners: `application` + `engine`

## Context

System model:

`many client threads -> one shared Exchange -> one shared MarketDispatcher -> one worker thread per market`

The exchange must stay correct under concurrent API calls while preserving matching semantics and wallet/accounting invariants.

## Non-Negotiable Invariants

1. Wallet balances are never negative:
   - `free >= 0`
   - `reserved >= 0`
2. No resting order exists without owner and market metadata.
3. `cancel_order` releases exactly the remaining reservation:
   - buy order: `remaining_quantity * price` in quote asset,
   - sell order: `remaining_quantity` in base asset.
4. Price-time priority is preserved inside a market:
   - better price first,
   - FIFO inside the same price level.

## Consistency Model

1. Per-market sequential consistency:
   - each market is processed by exactly one `MarketWorker`,
   - tasks for one market are executed in queue order.
2. Business-atomic operation outcome:
   - `place_limit`, `execute_market_order`, and `cancel_order` return a fully settled final state (or an error + rollback),
   - no externally visible partially settled state is allowed at API boundary.

## State Ownership

### Exchange owns

- accounts map (`UserId -> Account`)
- account structure mutex (`accounts_mu_`)
- per-account wallet mutex (`Account::mu`)
- id generators (+ generator mutexes)
- `OrderMetaStore` (sharded internal locking)
- `TradeHistory` (sharded internal locking)
- one `MarketDispatcher`

### MarketDispatcher owns

- `unordered_map<Market, MarketWorker>`
- workers lifecycle (`register`, `stop_all`)
- routing from market to worker

### MarketWorker owns

- one market-local order book
- one task queue + worker thread
- serial execution of market tasks (`Submit`, `Cancel`, `BestBid`, `BestAsk`, `Stop`)

## Lock Ordering Rules

1. Never hold `accounts_mu_` while touching `Wallet`.
   - `accounts_mu_` is only for account pointer lookup.
2. Settlement touching 2 accounts must lock by ascending `UserId`.
   - helper contract: `lock_two_accounts(a_id, a, b_id, b)`
3. Never hold account/store locks during `future.get()`.
4. Do not mix lock order ad-hoc across methods.
   - account locks follow the same deterministic order everywhere.
5. `OrderMetaStore::find` result should be treated as a snapshot for the current step.
   - avoid repeated `find` for the same order in one logical action.

## Operation Flows (High Level)

### place_limit_order

1. Validate input.
2. Lookup taker account under `accounts_mu_`, release `accounts_mu_`.
3. Lock taker account, reserve funds, unlock.
4. Generate `order_id`, persist meta in `OrderMetaStore`.
5. Submit to market worker and wait on `future.get()` (no account/store lock held).
6. On submit error: rollback reservation + erase meta.
7. For each execution:
   - read buyer/seller metadata,
   - lookup account pointers,
   - lock buyer/seller in `UserId` order,
   - settle wallets,
   - append trade,
   - erase fully-filled resting order metadata.

### execute_market_order

1. Validate input.
2. Lock taker account and reserve funds.
3. Submit to worker and wait on `future.get()` (no account/store lock held).
4. On submit error: rollback full taker reservation.
5. Settle executions with deterministic two-account locking.
6. Release unused taker reservation remainder.

### cancel_order

1. Verify user exists.
2. Read order metadata snapshot once.
3. Validate owner.
4. Submit cancel to worker and wait on `future.get()` (no account/store lock held).
5. On success:
   - lock owner account,
   - release exact remaining reservation,
   - erase metadata entry.

## Failure Semantics

1. Engine async errors map to typed application errors.
2. Reservation rollback on submit failure is mandatory.
3. Invariant-violating situations are treated as programmer errors (assertions in debug).

## Consequences

Positive:

- safe concurrent use of one shared `Exchange`,
- market parallelism (independent worker per market),
- deterministic deadlock prevention for settlement.

Tradeoffs:

- more complex lock discipline,
- extra metadata/store management for correctness under concurrency.

## Verification Requirements

Minimum required test coverage:

1. cancel-vs-fill stress does not deadlock (timeout-guarded).
2. parallel place-limit stress keeps balances non-negative.
3. mixed market-order + cancel preserves reserve/accounting invariants.
4. regression: full existing test suite stays green.
