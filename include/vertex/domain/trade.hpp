#pragma once
#include "vertex/core/types.hpp"

namespace vertex::domain
{

    using TradeId = vertex::core::TradeId;
    using OrderId = vertex::core::OrderId;
    using Price = vertex::core::Price;
    using Quantity = vertex::core::Quantity;
    using Symbol = vertex::core::Symbol;
    class Trade final
    {
    private:
        const TradeId trade_id_;
        const OrderId buy_order_id_;
        const OrderId sell_order_id_;
        const Symbol symbol_;
        const Quantity quantity_;
        const Price price_;

    public:
        Trade(TradeId trade_id, OrderId buy_order_id, OrderId sell_order_id, Symbol symbol, Quantity quantity, Price price);

        TradeId id() const noexcept;
        OrderId buy_order_id() const noexcept;
        OrderId sell_order_id() const noexcept;
        const Symbol &symbol() const noexcept;
        Quantity quantity() const noexcept;
        Price price() const noexcept;
    };
}