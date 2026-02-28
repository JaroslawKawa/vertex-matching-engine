#pragma once
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
#include "vertex/core/types.hpp"
#include "vertex/domain/order.hpp"
#include "vertex/domain/limit_order.hpp"
#include "vertex/domain/market_order.hpp"
#include "vertex/engine/resting_order.hpp"

namespace vertex::engine
{
    using Market = vertex::core::Market;
    using Quantity = vertex::core::Quantity;
    using Side = vertex::core::Side;
    using Price = vertex::core::Price;
    using OrderId = vertex::core::OrderId;
    using LimitOrder = vertex::domain::LimitOrder;
    using MarketOrder = vertex::domain::MarketOrder;

    struct OrderLocation
    {
        Side side;
        Price price;
        std::list<RestingOrder>::iterator it;
    };

    struct PriceLevel
    {
        std::list<RestingOrder> orders;
    };

    struct Execution
    {
        OrderId buy_order_id;
        OrderId sell_order_id;
        Quantity quantity;
        Price execution_price;
        Price buy_order_limit_price;
        bool buy_fully_filled;
        bool sell_fully_filled;
    };
    struct CancelResult
    {
        OrderId id;
        Side side;
        Price price;
        Quantity remaining_quantity;
    };
    class OrderBook
    {
    private:
        const Market market_;
        std::map<Price, PriceLevel, std::greater<>> bids_{}; // buyers list
        std::map<Price, PriceLevel, std::less<>> asks_{};    // seller list
        std::unordered_map<OrderId, OrderLocation> index_{};

    public:
        explicit OrderBook(Market market);
        std::vector<Execution> add_limit_order(std::unique_ptr<LimitOrder> order);
        std::vector<Execution> execute_market_order(std::unique_ptr<MarketOrder> order);
        std::optional<CancelResult> cancel(OrderId order_id);
        std::optional<Price> best_bid() const;
        std::optional<Price> best_ask() const;

        void insert_resting(Side side, const RestingOrder &order);
        std::vector<Execution> match_limit_buy_against_asks(const OrderId taker_order_id, const Price limit_price, Quantity &remaining_base_quantity);
        std::vector<Execution> match_limit_sell_against_bids(const OrderId taker_order_id, const Price limit_price, Quantity &remaining_base_quantity);
    };

}