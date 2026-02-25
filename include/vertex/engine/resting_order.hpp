#pragma once

#include <cassert>
#include "vertex/core/types.hpp"
namespace vertex::engine
{
    using OrderId = vertex::core::OrderId;
    using Market = vertex::core::Market;
    using Side = vertex::core::Side;
    using Price = vertex::core::Price;
    using Quantity = vertex::core::Quantity;

    struct RestingOrder
    {
        OrderId order_id;
        Price limit_price;
        Quantity initial_base_quantity;
        Quantity remaining_base_quantity;

        void reduce(Quantity executed)
        {
            assert(executed > 0);
            assert(executed <= remaining_base_quantity);
            remaining_base_quantity -= executed;
        }
        bool is_filled() const noexcept
        {
            return remaining_base_quantity == 0;
        }
    };

}