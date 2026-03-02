#pragma once

#include <unordered_map>
#include <unordered_set>
#include <expected>
#include <string>
#include "vertex/application/trade_history.hpp"
#include "vertex/core/id_generator.hpp"
#include "vertex/core/types.hpp"
#include "vertex/domain/trade.hpp"
#include "vertex/domain/user.hpp"
#include "vertex/domain/wallet.hpp"
#include "vertex/engine/matching_engine.hpp"
#include "vertex/engine/order_request.hpp"
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
    using MatchingEngine = vertex::engine::MatchingEngine;
    using Market = vertex::core::Market;
    using Execution = vertex::engine::Execution;
    using Trade = vertex::domain::Trade;
    using LimitOrderRequest = vertex::engine::LimitOrderRequest;
    using MarketBuyByQuoteRequest = vertex::engine::MarketBuyByQuoteRequest;
    using MarketSellByBaseRequest = vertex::engine::MarketSellByBaseRequest;
    
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
        WalletNotFound,
        InsufficientFunds,
        InvalidQuantity,
        InvalidAmount
    };
    enum class CancelOrderError
    {
        UserNotFound,
        OrderNotFound,
        NotOrderOwner
    };
    enum class RegisterMarketError
    {
        AlreadyListed,
        InvalidMarket
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
    class Exchange
    {
    private:
        std::unordered_map<UserId, User> users_;
        std::unordered_map<UserId, Wallet> wallets_;
        std::unordered_map<OrderId, UserId> orders_;
        std::unordered_map<OrderId, Market> orders_market_;

        UserIdGenerator user_id_generator_;
        OrderIdGenerator order_id_generator_;
        TradeIdGenerator trade_id_generator_;

        MatchingEngine matching_engine_{};
        TradeHistory trade_history_{};

        OrderPlacementResult execute_market_buy_by_quote(const UserId user_id, const Market &market, const Quantity order_quantity);
        OrderPlacementResult execute_market_sell_by_base(const UserId user_id, const Market &market, const Quantity order_quantity);
        
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
