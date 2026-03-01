# CLI Layer

This document reflects the current implementation in `app/main.cpp` and `app/cli/*`.

## Scope

CLI layer converts text commands into `Exchange` calls and prints user-facing output.

Main pieces:

- `command.hpp`: command AST (`std::variant` of all supported commands),
- `parse_error.hpp`: parse error model (`ParseStage`, `ParseErrorCode`, `ParseError`),
- `tokenizer.hpp/.cpp`: token stream with source column indexing,
- `parser.hpp/.cpp`: `parse_command(std::string_view)` -> `std::expected<Command, ParseError>`,
- `cli_app.hpp/.cpp`: `dispatch(const Command&)` -> `DispatchResult` and mapping to/from domain/application types,
- `printer.hpp/.cpp`: help text and output formatting for parse/dispatch results,
- `app/main.cpp`: REPL loop (`getline` -> parse -> dispatch -> print).

## Supported Commands

- `help`
- `exit`
- `create-user <name>`
- `get-user <user_id>`
- `deposit <user_id> <asset> <quantity>`
- `withdraw <user_id> <asset> <quantity>`
- `free-balance <user_id> <asset>`
- `reserved-balance <user_id> <asset>`
- `place-limit <user_id> <base>/<quote> <buy|sell> <price> <quantity>`
- `place-market <user_id> <base>/<quote> <buy|sell> <quantity>`
- `cancel-order <user_id> <order_id>`
- `register-market <base>/<quote>`

## Tokenizer Rules

- Splits by ASCII whitespace.
- Supports quoted tokens: `"Alice Bob"` is a single token.
- No escape-sequence handling inside quotes.
- Returns tokenizer-stage errors:
`EmptyLine`, `UnterminatedQuote`, `UnexpectedCharacterAfterQuote`.
- `Token.index` is a zero-based column index in input line.

## Parser Rules

- First token selects command; unknown root returns `UnknownCommand`.
- Validates argument count (`MissingArgument`, `TooManyArguments`).
- Validates ids (`uint64`), numbers (`int64`), market format (`<base>/<quote>`), and side (`buy|sell`).
- Side validation is case-insensitive (`buy`, `Buy`, `SELL`, etc. are accepted).
- Returns `ParseError{stage=Parser, code, message, column}` on validation failure.
- Only `parse_command` is exposed publicly; helper validators/parsers stay internal in `parser.cpp`.

## Dispatcher Contract (`CliApp`)

`CliApp::dispatch` maps AST commands to `Exchange` calls and returns `DispatchResult` (`std::variant`) with:

- info/control: `HelpRequested`, `ExitRequested`,
- success payloads: `UserCreated`, `UserRead`, `DepositDone`, `WithdrawDone`, `FreeBalanceRead`, `ReservedBalanceRead`, `LimitOrderPlaced`, `MarketOrderExecuted`, `OrderCanceled`, `MarketRegistered`,
- failure payload: `AppError{AppErrorCode, message}`.

`CliApp` converts string-level CLI values into domain/application types (`UserId`, `OrderId`, `Asset`, `Market`, `Side`) and converts some outputs back to strings for printing.

## Printer Contract

- `print_help(std::ostream&)`: prints static command help.
- `print_parse_error(const ParseError&, std::ostream&)`: prints stage, code, column, message.
- `print_dispatch_result(const DispatchResult&, std::ostream&)`: `std::visit` over all result variants and prints final line(s).

## Current REPL Flow

`main` does:

1. print boot banner and help,
2. loop on `std::getline`,
3. parse input,
4. print parse error or dispatch result,
5. break when `ExitRequested`.
