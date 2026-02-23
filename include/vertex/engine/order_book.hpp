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
namespace vertex::engine
{
    using Symbol = vertex::core::Symbol;
    using Quantity = vertex::core::Quantity;
    using Side = vertex::core::Side;
    using Price = vertex::core::Price;
    using OrderId = vertex::core::OrderId;
    using Order = vertex::domain::Order;

    struct OrderLocation
    {
        Side side;
        Price price;
        std::list<std::unique_ptr<Order>>::iterator it;
    };

    struct PriceLevel
    {
        std::list<std::unique_ptr<Order>> orders;
    };

    struct Execution
    {
        OrderId buy_order_id;
        OrderId sell_order_id;
        Quantity quantity;
        Price price;
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
        const Symbol symbol_;
        std::map<Price, PriceLevel, std::greater<>> bids_{}; // buyers list
        std::map<Price, PriceLevel, std::less<>> asks_{};    // seller list
        std::unordered_map<OrderId, OrderLocation> index_{};

    public:
        explicit OrderBook(Symbol symbol);
        std::vector<Execution> add_order(std::unique_ptr<Order> order);
        std::optional<CancelResult> cancel(OrderId order_id);
        std::optional<Price> best_bid() const;
        std::optional<Price> best_ask() const;
        bool empty() const noexcept;
    };

}