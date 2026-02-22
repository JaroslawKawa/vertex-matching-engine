#pragma once

#include "vertex/core/types.hpp"

namespace vertex::domain
{
    using OrderId = vertex::core::OrderId;
    using UserId = vertex::core::UserId;
    using Symbol = vertex::core::Symbol;
    using Quantity = vertex::core::Quantity;
    using Side = vertex::core::Side;
    using Price = vertex::core::Price;

    class Order
    {
    protected:
        OrderId order_id_;
        UserId user_id_;
        Symbol symbol_;
        Side side_;
        Quantity initial_quantity_;
        Quantity remaining_quantity_;

        Order(OrderId order_id, UserId user_id, Symbol symbol, Side side, Quantity initial_quantity);

    public:
        OrderId id() const noexcept;
        UserId user_id() const noexcept;
        const Symbol &symbol() const noexcept;
        Side side() const noexcept;

        Quantity initial_quantity() const noexcept;
        Quantity remaining_quantity() const noexcept;

        bool is_filled() const noexcept;
        bool is_active() const noexcept;

        void reduce(Quantity executed);

        virtual Price price() const = 0;
        virtual ~Order() = default;
    };
}
