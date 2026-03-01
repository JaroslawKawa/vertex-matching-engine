#include <format>
#include <string_view>
#include "printer.hpp"
namespace vertex::cli
{

    constexpr std::string_view kHelpText =
        "Vertex Matching Engine CLI\n"
        "\n"
        "Commands:\n"
        "  help\n"
        "  exit\n"
        "  create-user <name>\n"
        "  get-user <user_id>\n"
        "  deposit <user_id> <asset> <quantity>\n"
        "  withdraw <user_id> <asset> <quantity>\n"
        "  free-balance <user_id> <asset>\n"
        "  reserved-balance <user_id> <asset>\n"
        "  place-limit <user_id> <base>/<quote> <buy|sell> <price> <quantity>\n"
        "  place-market <user_id> <base>/<quote> <buy|sell> <quantity>\n"
        "  cancel-order <user_id> <order_id>\n"
        "  register-market <base>/<quote>\n"
        "\n"
        "Examples:\n"
        "  create-user Alice\n"
        "  register-market BTC/USDT\n"
        "  deposit 1 USDT 100000\n"
        "  place-limit 1 BTC/USDT buy 95000 2\n"
        "  place-market 1 BTC/USDT sell 1\n"
        "  cancel-order 1 42\n";

    template <class... Ts>
    struct Overloaded : Ts...
    {
        using Ts::operator()...;
    };
    template <class... Ts>
    Overloaded(Ts...) -> Overloaded<Ts...>;

    void Printer::print_help(std::ostream &stream)
    {
        stream << kHelpText;
    }

    void Printer::print_parse_error(const ParseError &error, std::ostream &stream)
    {

        stream << std::format("[ERROR] [{}] [{}] At postion {}: {}",
                              to_string(error.stage),
                              to_string(error.code),
                              error.column,
                              error.message);
    }

    void Printer::print_dispatch_result(const DispatchResult &result, std::ostream &stream)
    {
        return std::visit(
            Overloaded{
                [&stream](const ExitRequested &result) -> void
                {
                    stream << "[INFO] Exit requested";
                },
                [this, &stream](const HelpRequested &result) -> void
                {
                    print_help(stream);
                },
                [&stream](const UserCreated &result) -> void
                {
                    stream << std::format("[SUCCESS] User created: id={} name={}", result.user_id, result.name);
                },
                [&stream](const UserRead &result) -> void
                {
                    stream << std::format("[SUCCESS] User: id={} name={}", result.user_id, result.name);
                },
                [&stream](const DepositDone &result) -> void
                {
                    stream << std::format("[SUCCESS] Deposited {} {} to user {}", result.amount, result.asset, result.user_id);
                },
                [&stream](const WithdrawDone &result) -> void
                {
                    stream << std::format("[SUCCESS] Withdrew {} {} from user {}", result.amount, result.asset, result.user_id);
                },
                [&stream](const FreeBalanceRead &result) -> void
                {
                    stream << std::format("[SUCCESS] Free balance: user={} asset={} amount={}", result.user_id, result.asset, result.free);
                },
                [&stream](const ReservedBalanceRead &result) -> void
                {
                    stream << std::format("[SUCCESS] Reserved balance: user={} asset={} amount={}", result.user_id, result.asset, result.reserved);
                },
                [&stream](const LimitOrderPlaced &result) -> void
                {
                    stream << std::format("[SUCCESS] Limit order placed: id={} filled={} remaining={}", result.order_id, result.filled, result.remaining);
                },
                [&stream](const MarketOrderExecuted &result) -> void
                {
                    stream << std::format("[SUCCESS] Market order executed: id={} filled={} remaining={}", result.order_id, result.filled, result.remaining);
                },
                [&stream](const OrderCanceled &result) -> void
                {
                    stream << std::format("[SUCCESS] {} order {} canceled. Remaining {}", result.side, result.order_id, result.remaining);
                },
                [&stream](const MarketRegistered &result) -> void
                {
                    stream << std::format("[SUCCESS] Market {} registered", result.market);
                },
                [this, &stream](const AppError &result) -> void
                {
                    stream << std::format("[ERROR][{}] {}", to_string(result.code), result.message);
                },
            },
            result);
    }

    std::string Printer::to_string(ParseStage stage)
    {

        switch (stage)
        {
        case ParseStage::Tokenizer:
            return "Tokenizer";

        case ParseStage::Parser:
            return "Parser";

        default:
            return "Invalid stage";
        }
    }

    std::string Printer::to_string(ParseErrorCode error)
    {

        switch (error)
        {
        case ParseErrorCode::EmptyLine:
            return "EmptyLine";

        case ParseErrorCode::InvalidToken:
            return "InvalidToken";

        case ParseErrorCode::UnterminatedQuote:
            return "UnterminatedQuote";

        case ParseErrorCode::UnexpectedCharacterAfterQuote:
            return "UnexpectedCharacterAfterQuote";

        case ParseErrorCode::UnknownCommand:
            return "UnknownCommand";

        case ParseErrorCode::MissingArgument:
            return "MissingArgument";

        case ParseErrorCode::TooManyArguments:
            return "TooManyArguments";

        case ParseErrorCode::InvalidName:
            return "InvalidName";

        case ParseErrorCode::InvalidNumber:
            return "InvalidNumber";

        case ParseErrorCode::InvalidId:
            return "InvalidId";

        case ParseErrorCode::InvalidAsset:
            return "InvalidAsset";

        case ParseErrorCode::InvalidMarket:
            return "InvalidMarket";

        case ParseErrorCode::InvalidSide:
            return "InvalidSide";

        default:
            return "Invalid error code";
        }
    }

    std::string Printer::to_string(AppErrorCode error)
    {
        switch (error)
        {
        case AppErrorCode::InvalidInput:
            return "InvalidInput";
        case AppErrorCode::UserNotFound:
            return "UserNotFound";
        case AppErrorCode::UserAlreadyExists:
            return "UserAlreadyExists";
        case AppErrorCode::EmptyName:
            return "EmptyName";
        case AppErrorCode::MarketNotListed:
            return "MarketNotListed";
        case AppErrorCode::MarketAlreadyListed:
            return "MarketAlreadyListed";
        case AppErrorCode::InsufficientFunds:
            return "InsufficientFunds";
        case AppErrorCode::InsufficientReserved:
            return "InsufficientReserved";
        case AppErrorCode::InvalidAmount:
            return "InvalidAmount";
        case AppErrorCode::InvalidQuantity:
            return "InvalidQuantity";
        case AppErrorCode::OrderNotFound:
            return "OrderNotFound";
        case AppErrorCode::NotOrderOwner:
            return "NotOrderOwner";
        case AppErrorCode::InternalError:
            return "InternalError";
        default:
            return "InternalError";
        }
    }
}
