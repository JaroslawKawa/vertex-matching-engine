#pragma once
#include <variant>
#include "vertex/application/exchange.hpp"
#include "vertex/core/types.hpp"
#include "command.hpp"

namespace vertex::cli
{

    using Exchange = vertex::application::Exchange;
    using UserId = vertex::core::UserId;
    using OrderId = vertex::core::OrderId;
    using Asset = vertex::core::Asset;
    using Market = vertex::core::Market;
    using Side = vertex::core::Side;

    using UserError = vertex::application::UserError;
    using WalletOperationError = vertex::application::WalletOperationError;
    using PlaceOrderError = vertex::application::PlaceOrderError;
    using CancelOrderError = vertex::application::CancelOrderError;
    using RegisterMarketError = vertex::application::RegisterMarketError;

    enum class AppErrorCode
    {
        InvalidInput,
        UserNotFound,
        UserAlreadyExists,
        EmptyName,
        MarketNotListed,
        MarketAlreadyListed,
        InsufficientFunds,
        InsufficientReserved,
        InvalidAmount,
        InvalidQuantity,
        OrderNotFound,
        NotOrderOwner,
        InternalError
    };

    struct AppError
    {
        AppErrorCode code;
        std::string message;
    };

    struct HelpRequested
    {
    };
    struct ExitRequested
    {
    };

    struct UserCreated
    {
        std::uint64_t user_id;
        std::string name;
    };

    struct UserRead
    {
        std::uint64_t user_id;
        std::string name;
    };

    struct DepositDone
    {
        std::uint64_t user_id;
        std::string asset;
        std::int64_t amount;
    };

    struct WithdrawDone
    {
        std::uint64_t user_id;
        std::string asset;
        std::int64_t amount;
    };

    struct FreeBalanceRead
    {
        std::uint64_t user_id;
        std::string asset;
        std::int64_t free;
    };

    struct ReservedBalanceRead
    {
        std::uint64_t user_id;
        std::string asset;
        std::int64_t reserved;
    };

    struct LimitOrderPlaced
    {
        std::uint64_t order_id;
        std::int64_t filled;
        std::int64_t remaining;
    };

    struct MarketOrderExecuted
    {
        std::uint64_t order_id;
        std::int64_t filled;
        std::int64_t remaining;
    };

    struct OrderCanceled
    {
        std::uint64_t order_id;
        std::string side;
        std::int64_t remaining;
    };

    struct MarketRegistered
    {
        std::string market;
    };

    using DispatchResult = std::variant<
        ExitRequested,
        HelpRequested,
        UserCreated,
        UserRead,
        DepositDone,
        WithdrawDone,
        FreeBalanceRead,
        ReservedBalanceRead,
        LimitOrderPlaced,
        MarketOrderExecuted,
        OrderCanceled,
        MarketRegistered,
        AppError>;

    class CliApp
    {
    private:
        Exchange exchange_{};

        DispatchResult create_user(const CreateUser &cmd);
        DispatchResult get_user(const GetUser &cmd);
        DispatchResult deposit(const WalletDeposit &cmd);
        DispatchResult withdraw(const WalletWithdraw &cmd);
        DispatchResult free_balance(const WalletFreeBalance &cmd);
        DispatchResult reserved_balance(const WalletReservedBalance &cmd);
        DispatchResult place_limit_order(const PlaceLimitOrder &cmd);
        DispatchResult execute_market_order(const ExecuteMarketOrder &cmd);
        DispatchResult cancel_order(const CancelOrder &cmd);
        DispatchResult register_market(const RegisterMarket &cmd);

        Market parse_market(std::string_view market);
        Side parse_side(std::string_view side);
        std::string to_string(Side side);

    public:
        CliApp() = default;
        DispatchResult dispatch(const Command &command);
    };

}