#include <gtest/gtest.h>

#include "vertex/domain/limit_order.hpp"

namespace
{
    vertex::core::Market make_market()
    {
        const vertex::core::Asset base{"btc"};
        const vertex::core::Asset quote{"usdt"};
        EXPECT_NE(base, quote);
        return vertex::core::Market{base, quote};
    }
}

TEST(LimitOrderTest, ExposesConstructorData)
{
    vertex::domain::LimitOrder order{
        vertex::core::OrderId{100},
        vertex::core::UserId{7},
        make_market(),
        vertex::core::Side::Buy,
        25,
        12345};

    EXPECT_EQ(order.id(), vertex::core::OrderId{100});
    EXPECT_EQ(order.user_id(), vertex::core::UserId{7});
    EXPECT_EQ(order.side(), vertex::core::Side::Buy);
    EXPECT_EQ(order.initial_quantity(), 25);
    EXPECT_EQ(order.remaining_quantity(), 25);
    EXPECT_EQ(order.price(), 12345);
    EXPECT_TRUE(order.is_active());
    EXPECT_FALSE(order.is_filled());
}

TEST(LimitOrderTest, ReduceUpdatesRemainingAndFilledFlags)
{
    vertex::domain::LimitOrder order{
        vertex::core::OrderId{1},
        vertex::core::UserId{2},
        make_market(),
        vertex::core::Side::Sell,
        10,
        500};

    order.reduce(4);
    EXPECT_EQ(order.remaining_quantity(), 6);
    EXPECT_TRUE(order.is_active());
    EXPECT_FALSE(order.is_filled());

    order.reduce(6);
    EXPECT_EQ(order.remaining_quantity(), 0);
    EXPECT_FALSE(order.is_active());
    EXPECT_TRUE(order.is_filled());
}
