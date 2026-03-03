# Concurrency Test Matrix

This document maps concurrency/accounting invariants to concrete tests.

System model under test:

`many client threads -> one shared Exchange -> one shared MarketDispatcher -> one worker thread per market`

## Invariants

1. For each `(user, asset)`: `free >= 0`, `reserved >= 0`.
2. For each `(user, asset)`: `free + reserved` remains consistent with external operations (`deposit/withdraw` deltas).
3. In closed scenarios (no `deposit/withdraw` during stress), total asset amount across users is conserved.
4. No orphan open orders: every `OrderMetaStore` entry has existing owner and known market.
5. No deadlock/livelock under concurrent `place/cancel/market` traffic.
6. Cross-market isolation: heavy traffic on one market does not break another market.

## Test Mapping

### 1) Non-negative balances

- `ExchangeConcurrencyTest.PlaceLimitParallelStressDoesNotProduceNegativeBalances`
- `ExchangeConcurrencyTest.MarketOrderAndCancelMixedPreservesReserveInvariants`
- `ExchangeConcurrencyTest.PlaceLimitParallelOnTwoMarketsKeepsBalancesNonNegative`
- `ExchangeConcurrencyTest.CancelDuringHighInflowDoesNotDeadlockAndRestoresSellerReservation`

### 2) `free + reserved` consistency vs external operations

- `ExchangeConcurrencyTest.ConcurrentDepositWithdrawOnSameUserKeepsBalancesConsistent`
- `ExchangeConcurrencyTest.ConcurrentOperationsOnDifferentUsersStayIsolated`
- `ExchangeConcurrencyTest.SharedUsersAndDisjointUsersContentionScenariosStayConsistent`

### 3) Closed-scenario asset conservation

- `ExchangeConcurrencyTest.MarketOrderAndCancelMixedPreservesReserveInvariants`
- `ExchangeConcurrencyTest.HighTrafficOnBtcUsdtDoesNotBreakEthUsdtFlow`

### 4) No orphan open orders

- `ExchangeConcurrencyTest.PlaceLimitParallelStressDoesNotProduceNegativeBalances`
- `ExchangeConcurrencyTest.PlaceLimitParallelOnTwoMarketsKeepsBalancesNonNegative`
- `ExchangeConcurrencyTest.CancelDuringHighInflowDoesNotDeadlockAndRestoresSellerReservation`
- `ExchangeConcurrencyTest.HighTrafficOnBtcUsdtDoesNotBreakEthUsdtFlow`

Notes:

- these tests validate orphan-order invariant through test-only snapshot accessors:
  - `OrderMetaStoreTestAccess`
  - `ExchangeTestAccess`

### 5) Deadlock/livelock resistance

- `ExchangeConcurrencyTest.CancelVsFillDoesNotDeadlock`
- `ExchangeConcurrencyTest.CancelDuringHighInflowDoesNotDeadlockAndRestoresSellerReservation`
- `ExchangeConcurrencyTest.MarketOrderAndCancelMixedPreservesReserveInvariants`

Notes:

- stress tests use `TimeoutAbortGuard` to fail fast on hangs.
- threads are synchronized by `ThreadStartGate` (`std::latch`) to reduce startup skew.

### 6) Cross-market isolation

- `ExchangeConcurrencyTest.PlaceLimitParallelOnTwoMarketsKeepsBalancesNonNegative`
- `ExchangeConcurrencyTest.HighTrafficOnBtcUsdtDoesNotBreakEthUsdtFlow`

## Stability and Sanitizer Gates

### Local flaky hardening loop

Run `ctest` repeatedly:

```bash
for i in {1..20}; do ctest --test-dir build -C Debug --output-on-failure || exit 1; done
```

PowerShell:

```powershell
for ($i=1; $i -le 20; $i++) { ctest --test-dir build -C Debug --output-on-failure; if ($LASTEXITCODE -ne 0) { exit 1 } }
```

### TSAN gate (Linux only)

TSAN runs the same test suite with thread-sanitizer instrumentation.

Configure/build:

```bash
cmake -S . -B build-tsan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_COMPILER=clang++ -DVERTEX_ENABLE_TSAN=ON
cmake --build build-tsan --target tsan
```

CI should treat any TSAN report as failure.
