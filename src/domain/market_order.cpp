#include<cassert>
#include "vertex/domain/market_order.hpp"
namespace vertex::domain
{
    MarketOrder::MarketOrder(OrderId order_id, UserId user_id, Market market, Side side, Quantity initial_quantity) : Order(order_id, user_id, market, side, initial_quantity){};

    Price MarketOrder::price() const noexcept{
        assert(false && "MarketOrder has no price");
        return 0;
    }
}
