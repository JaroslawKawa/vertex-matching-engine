#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace vertex::cli
{
    struct Help
    {
    };
    struct Exit
    {
    };
    struct CreateUser
    {
        std::string name;
    };
    struct GetUser
    {
        std::uint64_t user_id;
    };

    struct WalletDeposit
    {
        std::uint64_t user_id;
        std::string asset;
        std::int64_t quantity;
    };

    struct WalletWithdraw
    {
        std::uint64_t user_id;
        std::string asset;
        std::int64_t quantity;
    };

    struct WalletFreeBalance
    {
        std::uint64_t user_id;
        std::string asset;
    };
    struct WalletReservedBalance
    {
        std::uint64_t user_id;
        std::string asset;
    };
    struct PlaceLimitOrder
    {
        std::uint64_t user_id;
        std::string market;
        std::string side;
        std::int64_t price;
        std::int64_t quantity;
    };
    struct ExecuteMarketOrder
    {
        std::uint64_t user_id;
        std::string market;
        std::string side;
        std::int64_t quantity;
    };
    struct CancelOrder
    {
        std::uint64_t user_id;
        std::uint64_t order_id;
    };
    struct RegisterMarket
    {
        std::string market;
    };

    using Command = std::variant<
        Help,
        Exit,
        CreateUser,
        GetUser,
        WalletDeposit,
        WalletWithdraw,
        WalletFreeBalance,
        WalletReservedBalance,
        PlaceLimitOrder,
        ExecuteMarketOrder,
        CancelOrder,
        RegisterMarket>;

}