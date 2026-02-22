#include <cassert>
#include <utility>
#include "vertex/domain/limit_order.hpp"

namespace vertex::domain
{

    LimitOrder::LimitOrder(OrderId order_id, UserId user_id, Symbol symbol, Side side, Quantity initial_quantity, Price price) : Order(order_id, user_id, std::move(symbol), side, initial_quantity),
                                                                                                                                 price_(price)
    {
        assert(price_ > 0);
    }

    Price LimitOrder::price() const noexcept
    {
        return price_;
    }

}