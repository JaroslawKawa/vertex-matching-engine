#include <gtest/gtest.h>

#include <memory>

#include "vertex/domain/limit_order.hpp"
#include "vertex/engine/matching_engine.hpp"

namespace
{
    using vertex::core::Asset;
    using vertex::core::Market;
    using vertex::core::OrderId;
    using vertex::core::Side;
    using vertex::domain::LimitOrder;
    using vertex::engine::MatchingEngine;
    using vertex::engine::Order;

    Market btc_usdt()
    {
        return Market{Asset{"btc"}, Asset{"usdt"}};
    }

    std::unique_ptr<Order> make_limit_order(
        OrderId order_id,
        vertex::core::UserId user_id,
        Side side,
        vertex::core::Quantity quantity,
        vertex::core::Price price)
    {
        return std::make_unique<LimitOrder>(order_id, user_id, btc_usdt(), side, quantity, price);
    }
}

TEST(MatchingEngineTest, RegisterMarketUpdatesPresence)
{
    MatchingEngine engine;

    EXPECT_FALSE(engine.has_market(btc_usdt()));
    engine.register_market(btc_usdt());
    EXPECT_TRUE(engine.has_market(btc_usdt()));
}

TEST(MatchingEngineTest, AddOrderRoutesToRegisteredOrderBook)
{
    MatchingEngine engine;
    engine.register_market(btc_usdt());

    EXPECT_TRUE(engine.add_order(make_limit_order(OrderId{100}, vertex::core::UserId{1}, Side::Sell, 5, 100)).empty());
    const auto executions = engine.add_order(make_limit_order(OrderId{101}, vertex::core::UserId{2}, Side::Buy, 5, 110));

    ASSERT_EQ(executions.size(), 1u);
    EXPECT_EQ(executions.front().buy_order_id, OrderId{101});
    EXPECT_EQ(executions.front().sell_order_id, OrderId{100});
    EXPECT_EQ(executions.front().execution_price, 100);
    EXPECT_EQ(executions.front().quantity, 5);
}

TEST(MatchingEngineTest, CancelRoutesToRegisteredOrderBook)
{
    MatchingEngine engine;
    engine.register_market(btc_usdt());

    EXPECT_TRUE(engine.add_order(make_limit_order(OrderId{200}, vertex::core::UserId{7}, Side::Buy, 8, 99)).empty());
    const auto cancel_result = engine.cancel(btc_usdt(), OrderId{200});

    ASSERT_TRUE(cancel_result.has_value());
    EXPECT_EQ(cancel_result->id, OrderId{200});
    EXPECT_EQ(cancel_result->side, Side::Buy);
    EXPECT_EQ(cancel_result->price, 99);
    EXPECT_EQ(cancel_result->remaining_quantity, 8);
}
