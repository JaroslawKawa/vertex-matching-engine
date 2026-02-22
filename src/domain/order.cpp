#include <cassert>
#include <utility>
#include "vertex/domain/order.hpp"

namespace vertex::domain
{

    Order::Order(OrderId order_id, UserId user_id, Symbol symbol, Side side, Quantity initial_quantity) : order_id_{order_id}, user_id_(user_id), symbol_(std::move(symbol)), side_(side), initial_quantity_(initial_quantity), remaining_quantity_(initial_quantity)
    {
        assert(order_id.is_valid());
        assert(user_id.is_valid());
        assert(initial_quantity > 0);
    }

    OrderId Order::id() const noexcept
    {
        return order_id_;
    }

    UserId Order::user_id() const noexcept
    {
        return user_id_;
    }

    const Symbol &Order::symbol() const noexcept
    {
        return symbol_;
    }

    Side Order::side() const noexcept
    {
        return side_;
    }

    Quantity Order::initial_quantity() const noexcept
    {
        return initial_quantity_;
    }

    Quantity Order::remaining_quantity() const noexcept
    {
        return remaining_quantity_;
    }

    bool Order::is_filled() const noexcept
    {
        return remaining_quantity_ == 0;
    }

    bool Order::is_active() const noexcept
    {
        return remaining_quantity_ > 0;
    }

    void Order::reduce(Quantity executed)
    {
        assert(executed > 0);
        assert(executed <= remaining_quantity_);
        remaining_quantity_ -= executed;
    }

}