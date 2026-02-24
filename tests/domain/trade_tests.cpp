#include <gtest/gtest.h>

#include "vertex/domain/trade.hpp"

namespace
{
    vertex::core::Market make_market()
    {
        return vertex::core::Market{vertex::core::Asset{"eth"}, vertex::core::Asset{"usdt"}};
    }
}

TEST(TradeTest, ExposesAllImmutableFields)
{
    vertex::domain::Trade trade{
        vertex::core::TradeId{77},
        vertex::core::UserId{11},
        vertex::core::UserId{22},
        vertex::core::OrderId{101},
        vertex::core::OrderId{202},
        make_market(),
        3,
        2500};

    EXPECT_EQ(trade.id(), vertex::core::TradeId{77});
    EXPECT_EQ(trade.buy_user_id(), vertex::core::UserId{11});
    EXPECT_EQ(trade.sell_user_id(), vertex::core::UserId{22});
    EXPECT_EQ(trade.buy_order_id(), vertex::core::OrderId{101});
    EXPECT_EQ(trade.sell_order_id(), vertex::core::OrderId{202});
    EXPECT_EQ(trade.market().base(), vertex::core::Asset{"ETH"});
    EXPECT_EQ(trade.market().quote(), vertex::core::Asset{"USDT"});
    EXPECT_EQ(trade.quantity(), 3);
    EXPECT_EQ(trade.price(), 2500);
}