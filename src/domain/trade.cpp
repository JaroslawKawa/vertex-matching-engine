#include "vertex/domain/trade.hpp"
#include <cassert>
namespace vertex::domain
{

    Trade::Trade(TradeId trade_id, UserId buy_user_id, UserId sell_user_id, OrderId buy_order_id, OrderId sell_order_id, Market market, Quantity quantity, Price price) : trade_id_(trade_id),
                                                                                                                                                                          buy_user_id_(buy_user_id),
                                                                                                                                                                          sell_user_id_(sell_user_id),
                                                                                                                                                                          buy_order_id_(buy_order_id),
                                                                                                                                                                          sell_order_id_(sell_order_id),
                                                                                                                                                                          market_(market),
                                                                                                                                                                          quantity_(quantity),
                                                                                                                                                                          price_(price)
    {
        assert(trade_id_.is_valid());
        assert(buy_order_id_.is_valid());
        assert(sell_order_id_.is_valid());
        assert(sell_order_id_ != buy_order_id_);
        assert(quantity > 0);
        assert(price > 0);
    }

    TradeId Trade::id() const noexcept
    {
        return trade_id_;
    }
    UserId Trade::buy_user_id() const noexcept
    {
        return buy_user_id_;
    }
    UserId Trade::sell_user_id() const noexcept
    {
        return sell_user_id_;
    }
    OrderId Trade::buy_order_id() const noexcept
    {
        return buy_order_id_;
    }

    OrderId Trade::sell_order_id() const noexcept
    {
        return sell_order_id_;
    }

    const Market &Trade::market() const noexcept
    {
        return market_;
    }

    Quantity Trade::quantity() const noexcept
    {
        return quantity_;
    }

    Price Trade::price() const noexcept
    {
        return price_;
    }
}