#pragma once
#include "vertex/domain/order.hpp"

namespace vertex::domain
{
class MarketOrder final : public  Order
{
private:
    /* data */
public:
    MarketOrder(OrderId order_id, UserId user_id, Market market, Side side, Quantity initial_quantity);
    Price price() const noexcept override;
};


}