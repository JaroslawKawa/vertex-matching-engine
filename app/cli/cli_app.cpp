#include "cli_app.hpp"
#include "vertex/application/exchange.hpp"
#include <cassert>

namespace vertex::cli
{
    template <class... Ts>
    struct Overloaded : Ts...
    {
        using Ts::operator()...;
    };
    template <class... Ts>
    Overloaded(Ts...) -> Overloaded<Ts...>;

    DispatchResult CliApp::create_user(const CreateUser &cmd)
    {
        auto result = exchange_.create_user(cmd.name);

        if (!result)
        {
            switch (result.error())
            {
            case UserError::EmptyName:
                return AppError{.code = AppErrorCode::EmptyName,
                                .message = "Empty user name"};

            case UserError::UserAlreadyExists:
                return AppError{.code = AppErrorCode::UserAlreadyExists,
                                .message = "User already exists"};

            default:
                return AppError{.code = AppErrorCode::InternalError,
                                .message = "Internal error"};
            }
        }

        return UserCreated{.user_id = result.value().get_value(),
                           .name = cmd.name};
    }

    DispatchResult CliApp::get_user(const GetUser &cmd)
    {
        UserId user_id{cmd.user_id};

        auto result = exchange_.get_user_name(user_id);

        if (!result)
        {

            switch (result.error())
            {
            case UserError::UserNotFound:
                return AppError{.code = AppErrorCode::UserNotFound,
                                .message = "User not found"};

            default:
                return AppError{.code = AppErrorCode::InternalError,
                                .message = "Internal error"};
            }
        }

        return UserRead{.user_id = cmd.user_id,
                        .name = result.value()};
    }

    DispatchResult CliApp::deposit(const WalletDeposit &cmd)
    {

        UserId user_id{cmd.user_id};
        Asset asset{cmd.asset};

        auto result = exchange_.deposit(user_id, asset, cmd.quantity);

        if (!result)
        {

            switch (result.error())
            {
            case WalletOperationError::UserNotFound:
                return AppError{.code = AppErrorCode::UserNotFound,
                                .message = "User not found"};

            case WalletOperationError::InvalidQuantity:
                return AppError{.code = AppErrorCode::InvalidQuantity,
                                .message = "Invalid quantity"};

            default:
                return AppError{.code = AppErrorCode::InternalError,
                                .message = "Internal error"};
            }
        }

        return DepositDone{.user_id = cmd.user_id,
                           .asset = cmd.asset,
                           .amount = cmd.quantity};
    }

    DispatchResult CliApp::withdraw(const WalletWithdraw &cmd)
    {

        UserId user_id{cmd.user_id};
        Asset asset{cmd.asset};

        auto result = exchange_.withdraw(user_id, asset, cmd.quantity);

        if (!result)
        {

            switch (result.error())
            {
            case WalletOperationError::UserNotFound:
                return AppError{.code = AppErrorCode::UserNotFound,
                                .message = "User not found"};

            case WalletOperationError::InsufficientFunds:
                return AppError{.code = AppErrorCode::InsufficientFunds,
                                .message = "Insufficient funds"};

            case WalletOperationError::InvalidQuantity:
                return AppError{.code = AppErrorCode::InvalidQuantity,
                                .message = "Invalid quantity"};

            default:
                return AppError{.code = AppErrorCode::InternalError,
                                .message = "Internal error"};
            }
        }

        return WithdrawDone{.user_id = cmd.user_id,
                            .asset = cmd.asset,
                            .amount = cmd.quantity};
    }

    DispatchResult CliApp::free_balance(const WalletFreeBalance &cmd)
    {

        UserId user_id{cmd.user_id};
        Asset asset{cmd.asset};

        auto result = exchange_.free_balance(user_id, asset);

        if (!result)
        {

            switch (result.error())
            {
            case WalletOperationError::UserNotFound:
                return AppError{.code = AppErrorCode::UserNotFound,
                                .message = "User not found"};

            default:
                return AppError{.code = AppErrorCode::InternalError,
                                .message = "Internal error"};
            }
        }

        return FreeBalanceRead{.user_id = cmd.user_id,
                               .asset = cmd.asset,
                               .free = result.value()};
    }

    DispatchResult CliApp::reserved_balance(const WalletReservedBalance &cmd)
    {

        UserId user_id{cmd.user_id};
        Asset asset{cmd.asset};

        auto result = exchange_.reserved_balance(user_id, asset);

        if (!result)
        {

            switch (result.error())
            {
            case WalletOperationError::UserNotFound:
                return AppError{.code = AppErrorCode::UserNotFound,
                                .message = "User not found"};

            default:
                return AppError{.code = AppErrorCode::InternalError,
                                .message = "Internal error"};
            }
        }

        return ReservedBalanceRead{.user_id = cmd.user_id,
                                   .asset = cmd.asset,
                                   .reserved = result.value()};
    }

    DispatchResult CliApp::place_limit_order(const PlaceLimitOrder &cmd)
    {
        UserId user_id{cmd.user_id};
        Market market = parse_market(cmd.market);
        Side side = parse_side(cmd.side);

        auto result = exchange_.place_limit_order(user_id, market, side, cmd.price, cmd.quantity);

        if (!result)
        {

            switch (result.error())
            {
            case PlaceOrderError::UserNotFound:
                return AppError{.code = AppErrorCode::UserNotFound,
                                .message = "User not found"};

            case PlaceOrderError::MarketNotListed:
                return AppError{.code = AppErrorCode::MarketNotListed,
                                .message = "Market not listed"};

            case PlaceOrderError::InvalidQuantity:
                return AppError{.code = AppErrorCode::InvalidQuantity,
                                .message = "Invalid quantity"};

            case PlaceOrderError::InvalidAmount:
                return AppError{.code = AppErrorCode::InvalidAmount,
                                .message = "Invalid amount"};

            case PlaceOrderError::InsufficientFunds:
                return AppError{.code = AppErrorCode::InsufficientFunds,
                                .message = "Insufficient funds"};

            default:
                return AppError{.code = AppErrorCode::InternalError,
                                .message = "Internal error"};
            }
        }

        return LimitOrderPlaced{.order_id = result.value().order_id.get_value(),
                                .filled = result.value().filled_quantity,
                                .remaining = result.value().remaining_quantity};
    }

    DispatchResult CliApp::execute_market_order(const ExecuteMarketOrder &cmd)
    {
        UserId user_id{cmd.user_id};
        Market market = parse_market(cmd.market);
        Side side = parse_side(cmd.side);

        auto result = exchange_.execute_market_order(user_id, market, side, cmd.quantity);

        if (!result)
        {

            switch (result.error())
            {
            case PlaceOrderError::UserNotFound:
                return AppError{.code = AppErrorCode::UserNotFound,
                                .message = "User not found"};

            case PlaceOrderError::MarketNotListed:
                return AppError{.code = AppErrorCode::MarketNotListed,
                                .message = "Market not listed"};

            case PlaceOrderError::InvalidQuantity:
                return AppError{.code = AppErrorCode::InvalidQuantity,
                                .message = "Invalid quantity"};

            case PlaceOrderError::InsufficientFunds:
                return AppError{.code = AppErrorCode::InsufficientFunds,
                                .message = "Insufficient funds"};

            default:
                return AppError{.code = AppErrorCode::InternalError,
                                .message = "Internal error"};
            }
        }

        return MarketOrderExecuted{.order_id = result.value().order_id.get_value(),
                                   .filled = result.value().filled_quantity,
                                   .remaining = result.value().remaining_quantity};
    }

    DispatchResult CliApp::cancel_order(const CancelOrder &cmd)
    {
        UserId user_id{cmd.user_id};
        OrderId order_id{cmd.order_id};

        auto result = exchange_.cancel_order(user_id, order_id);

        if (!result)
        {

            switch (result.error())
            {
            case CancelOrderError::UserNotFound:
                return AppError{.code = AppErrorCode::UserNotFound,
                                .message = "User not found"};

            case CancelOrderError::OrderNotFound:
                return AppError{.code = AppErrorCode::OrderNotFound,
                                .message = "Order not found"};

            case CancelOrderError::NotOrderOwner:
                return AppError{.code = AppErrorCode::NotOrderOwner,
                                .message = "Not order owner"};

            default:
                return AppError{.code = AppErrorCode::InternalError,
                                .message = "Internal error"};
            }
        }

        return OrderCanceled{.order_id = cmd.order_id,
                             .side = to_string(result.value().side),
                             .remaining = result.value().remaining_quantity};
    }

    DispatchResult CliApp::register_market(const RegisterMarket &cmd)
    {

        Market market = parse_market(cmd.market);

        auto result = exchange_.register_market(market);

        if (!result)
        {

            switch (result.error())
            {
            case RegisterMarketError::AlreadyListed:
                return AppError{.code = AppErrorCode::MarketAlreadyListed,
                                .message = "Market already listed"};

            default:
                return AppError{.code = AppErrorCode::InternalError,
                                .message = "Internal error"};
            }
        }

        return MarketRegistered{.market = cmd.market};
    }

    DispatchResult CliApp::dispatch(const Command &command)
    {
        return std::visit(
            Overloaded{
                [this](const Help &command) -> DispatchResult
                {
                    return HelpRequested{};
                },
                [this](const Exit &command) -> DispatchResult
                {
                    return ExitRequested{};
                },
                [this](const CreateUser &command) -> DispatchResult
                {
                    return create_user(command);
                },
                [this](const GetUser &command) -> DispatchResult
                {
                    return get_user(command);
                },
                [this](const WalletDeposit &command) -> DispatchResult
                {
                    return deposit(command);
                },
                [this](const WalletWithdraw &command) -> DispatchResult
                {
                    return withdraw(command);
                },
                [this](const WalletFreeBalance &command) -> DispatchResult
                {
                    return free_balance(command);
                },
                [this](const WalletReservedBalance &command) -> DispatchResult
                {
                    return reserved_balance(command);
                },
                [this](const PlaceLimitOrder &command) -> DispatchResult
                {
                    return place_limit_order(command);
                },
                [this](const ExecuteMarketOrder &command) -> DispatchResult
                {
                    return execute_market_order(command);
                },
                [this](const CancelOrder &command) -> DispatchResult
                {
                    return cancel_order(command);
                },
                [this](const RegisterMarket &command) -> DispatchResult
                {
                    return register_market(command);
                },
            },
            command);
    }

    Market CliApp::parse_market(std::string_view market)
    {
        auto slash_it = market.find('/');
        auto second_slash = market.find('/', slash_it == std::string_view::npos ? 0 : slash_it + 1);

        assert(slash_it != std::string_view::npos && "Invariant violated: parser must provide market as <base>/<quote>");
        assert(slash_it > 0 && "Invariant violated: market base asset must be non-empty");
        assert(slash_it + 1 < market.size() && "Invariant violated: market quote asset must be non-empty");
        assert(second_slash == std::string_view::npos && "Invariant violated: market must contain exactly one slash");

        auto base = market.substr(0, slash_it);
        auto quote = market.substr(slash_it + 1);

        return Market{Asset{std::string(base)}, Asset{std::string(quote)}};
    }

    Side CliApp::parse_side(std::string_view side)
    {
        std::string s(side);

        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });

        assert((s == "buy" || s == "sell") && "Invariant violated: parser must provide side as buy/sell");

        return s == "buy" ? Side::Buy : Side::Sell;
    }

    std::string CliApp::to_string(Side side)
    {

        if (side == Side::Buy)
            return "Buy";
        else
            return "Sell";
    }
}
