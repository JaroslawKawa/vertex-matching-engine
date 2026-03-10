# ADR-0002: REST API Transport Model and Layer Boundaries

- Status: Accepted
- Date: 2026-03-10
- Owners: `application` + `http` (planned)

## Context

The system currently exposes a CLI interface over one shared `Exchange` instance.
We are adding a REST API and must keep existing application/domain boundaries stable while enabling concurrent request handling.

## Decision

1. `Exchange` remains unchanged as sync core.
   - Business logic and invariants stay in `application::Exchange`.
   - HTTP transport does not introduce domain logic.

2. API process uses one shared `std::shared_ptr<vertex::application::Exchange>`.
   - All HTTP handlers call the same shared instance.
   - Concurrency control remains inside `Exchange`/stores/engine workers.

3. REST transport is a separate module.
   - New code lives in `http` module (separate from `app/cli`).
   - CLI and REST are two independent adapters over the same application core.

4. API is versioned and has stable error envelope.
   - Endpoints use `/v1/...`.
   - Error responses use one consistent structure (`code` + `message`).

## Consequences

Positive:

- transport-agnostic application core,
- shared behavior between CLI and REST,
- clear ownership of concurrency/invariants in one place.

Tradeoffs:

- explicit mapping layer needed (`Exchange` errors -> HTTP status/body),
- additional module/build target and integration tests.