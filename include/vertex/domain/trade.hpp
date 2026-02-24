#pragma once
#include "vertex/core/types.hpp"

namespace vertex::domain
{

    using TradeId = vertex::core::TradeId;
    using OrderId = vertex::core::OrderId;
    using UserId = vertex::core::UserId;
    using Price = vertex::core::Price;
    using Quantity = vertex::core::Quantity;
    using Asset = vertex::core::Asset;
    using Market = vertex::core::Market;
    class Trade final
    {
    private:
        const TradeId trade_id_;
        const UserId buy_user_id_;
        const UserId sell_user_id_;
        const OrderId buy_order_id_;
        const OrderId sell_order_id_;
        const Market market_;
        const Quantity quantity_;
        const Price price_;

    public:
        Trade(TradeId trade_id, UserId buy_user_id, UserId sell_user_id, OrderId buy_order_id, OrderId sell_order_id, Market market, Quantity quantity, Price price);

        TradeId id() const noexcept;
        UserId buy_user_id() const noexcept;
        UserId sell_user_id() const noexcept;
        OrderId buy_order_id() const noexcept;
        OrderId sell_order_id() const noexcept;
        const Market &market() const noexcept;
        Quantity quantity() const noexcept;
        Price price() const noexcept;
    };
}