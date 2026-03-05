#pragma once

#include <unordered_map>
#include <unordered_set>
#include <expected>
#include <string>
#include "vertex/application/trade_history.hpp"
#include "vertex/application/order_meta_store.hpp"
#include "vertex/application/order_history.hpp"
#include "vertex/core/id_generator.hpp"
#include "vertex/core/types.hpp"
#include "vertex/domain/trade.hpp"
#include "vertex/domain/user.hpp"
#include "vertex/domain/wallet.hpp"
#include "vertex/engine/order_request.hpp"
#include "vertex/engine/market_dispatcher.hpp"
#include "vertex/engine/engine_async_error.hpp"

namespace vertex::application
{
    using TradeHistory = vertex::application::TradeHistory;
    using UserId = vertex::core::UserId;
    using UserIdGenerator = vertex::core::IdGenerator<UserId>;
    using User = vertex::domain::User;
    using OrderId = vertex::core::OrderId;
    using OrderIdGenerator = vertex::core::IdGenerator<OrderId>;
    using TradeId = vertex::core::TradeId;
    using TradeIdGenerator = vertex::core::IdGenerator<TradeId>;
    using Wallet = vertex::domain::Wallet;
    using Quantity = vertex::core::Quantity;
    using Asset = vertex::core::Asset;
    using Price = vertex::core::Price;
    using Side = vertex::core::Side;
    using MarketDispatcher = vertex::engine::MarketDispatcher;
    using Market = vertex::core::Market;
    using Execution = vertex::engine::Execution;
    using Trade = vertex::domain::Trade;
    using LimitOrderRequest = vertex::engine::LimitOrderRequest;
    using MarketBuyByQuoteRequest = vertex::engine::MarketBuyByQuoteRequest;
    using MarketSellByBaseRequest = vertex::engine::MarketSellByBaseRequest;
    using EngineAsyncError = vertex::engine::EngineAsyncError;
    using WalletError = vertex::domain::WalletError;

    enum class WalletOperationError
    {
        UserNotFound,
        InsufficientFunds,
        InsufficientReserved,
        InvalidQuantity

    };
    enum class UserError
    {
        UserNotFound,
        UserAlreadyExists,
        EmptyName
    };
    enum class PlaceOrderError
    {
        MarketNotListed,
        UserNotFound,
        InsufficientFunds,
        InvalidQuantity,
        InvalidAmount,
        WorkerStopped,
        OrderIdCollision
    };
    enum class CancelOrderError
    {
        UserNotFound,
        OrderNotFound,
        NotOrderOwner,
        MarketNotFound,
        WorkerStopped
    };
    enum class RegisterMarketError
    {
        AlreadyListed,
        WorkerStopped
    };
    struct OrderPlacementResult
    {
        OrderId order_id;
        Quantity filled_quantity;
        Quantity remaining_quantity;
    };
    struct CancelOrderResult
    {
        OrderId id;
        Side side;
        Quantity remaining_quantity;
    };

    struct Account
    {
        User user;
        Wallet wallet;
        std::mutex mu{};

        Account(User u, Wallet w)
            : user(std::move(u)),
              wallet(std::move(w))
        {
        }
    };

    class ExchangeTestAccess;

    class Exchange

    {
    private:
        friend class ExchangeTestAccess;

        OrderMetaStore order_meta_store_;

        std::unordered_map<UserId, std::shared_ptr<Account>> accounts_;
        mutable std::shared_mutex accounts_mu_;

        UserIdGenerator user_id_generator_;
        std::mutex user_id_generator_mu_;
        OrderIdGenerator order_id_generator_;
        std::mutex order_id_generator_mu_;
        TradeIdGenerator trade_id_generator_;
        std::mutex trade_id_generator_mu_;

        MarketDispatcher market_dispatcher_{};
        TradeHistory trade_history_{};
        OrderHistory order_history_{};

        std::expected<OrderPlacementResult, PlaceOrderError> execute_market_buy_by_quote(const UserId user_id, const Market &market, const Quantity order_quantity);
        std::expected<OrderPlacementResult, PlaceOrderError> execute_market_sell_by_base(const UserId user_id, const Market &market, const Quantity order_quantity);

    public:
        Exchange() = default;
        std::expected<UserId, UserError> create_user(std::string name);
        std::expected<std::string, UserError> get_user_name(const UserId user_id) const;
        bool user_exists(const UserId user_id) const;

        std::expected<void, WalletOperationError> deposit(const UserId user_id, const Asset &asset, const Quantity quantity);
        std::expected<void, WalletOperationError> withdraw(const UserId user_id, const Asset &asset, const Quantity quantity);
        std::expected<void, WalletOperationError> reserve(const UserId user_id, const Asset &asset, const Quantity quantity);
        std::expected<void, WalletOperationError> release(const UserId user_id, const Asset &asset, const Quantity quantity);
        std::expected<Quantity, WalletOperationError> free_balance(const UserId user_id, const Asset &asset) const;
        std::expected<Quantity, WalletOperationError> reserved_balance(const UserId user_id, const Asset &asset) const;

        std::expected<OrderPlacementResult, PlaceOrderError> place_limit_order(const UserId user_id, const Market &market, const Side side, const Price price, const Quantity quantity);
        std::expected<OrderPlacementResult, PlaceOrderError> execute_market_order(const UserId user_id, const Market &market, const Side side, const Quantity order_quantity);
        std::expected<CancelOrderResult, CancelOrderError> cancel_order(const UserId user_id, const OrderId order_id);
        std::expected<void, RegisterMarketError> register_market(const Market &market);
    };
} // namespace vertex::application
