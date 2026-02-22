#pragma once
#include "vertex/domain/order.hpp"
namespace vertex::domain
{

    class LimitOrder final : public Order
    {
    private:
        const Price price_;

    public:
        LimitOrder(OrderId order_id, UserId user_id, Symbol symbol, Side side, Quantity initial_quantity, Price price);
        Price price() const noexcept override;
    };

}