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

    Market eth_usdt()
    {
        return Market{Asset{"eth"}, Asset{"usdt"}};
    }

    std::unique_ptr<Order> make_limit_order(
        const Market &market,
        OrderId order_id,
        vertex::core::UserId user_id,
        Side side,
        vertex::core::Quantity quantity,
        vertex::core::Price price)
    {
        return std::make_unique<LimitOrder>(order_id, user_id, market, side, quantity, price);
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

    EXPECT_TRUE(engine.add_order(make_limit_order(btc_usdt(), OrderId{100}, vertex::core::UserId{1}, Side::Sell, 5, 100)).empty());
    const auto executions = engine.add_order(make_limit_order(btc_usdt(), OrderId{101}, vertex::core::UserId{2}, Side::Buy, 5, 110));

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

    EXPECT_TRUE(engine.add_order(make_limit_order(btc_usdt(), OrderId{200}, vertex::core::UserId{7}, Side::Buy, 8, 99)).empty());
    const auto cancel_result = engine.cancel(btc_usdt(), OrderId{200});

    ASSERT_TRUE(cancel_result.has_value());
    EXPECT_EQ(cancel_result->id, OrderId{200});
    EXPECT_EQ(cancel_result->side, Side::Buy);
    EXPECT_EQ(cancel_result->price, 99);
    EXPECT_EQ(cancel_result->remaining_quantity, 8);
}

TEST(MatchingEngineTest, UnknownOrderCancelReturnsNulloptForRegisteredMarket)
{
    MatchingEngine engine;
    engine.register_market(btc_usdt());

    EXPECT_FALSE(engine.cancel(btc_usdt(), OrderId{999}).has_value());
}

TEST(MatchingEngineTest, MarketsAreIsolatedFromEachOther)
{
    MatchingEngine engine;
    engine.register_market(btc_usdt());
    engine.register_market(eth_usdt());

    EXPECT_TRUE(engine.add_order(make_limit_order(btc_usdt(), OrderId{301}, vertex::core::UserId{11}, Side::Sell, 2, 100)).empty());
    EXPECT_TRUE(engine.add_order(make_limit_order(eth_usdt(), OrderId{401}, vertex::core::UserId{12}, Side::Buy, 2, 1000)).empty());

    const auto btc_exec = engine.add_order(make_limit_order(btc_usdt(), OrderId{302}, vertex::core::UserId{13}, Side::Buy, 2, 105));
    ASSERT_EQ(btc_exec.size(), 1u);
    EXPECT_EQ(btc_exec.front().sell_order_id, OrderId{301});

    const auto eth_exec = engine.add_order(make_limit_order(eth_usdt(), OrderId{402}, vertex::core::UserId{14}, Side::Sell, 1, 995));
    ASSERT_EQ(eth_exec.size(), 1u);
    EXPECT_EQ(eth_exec.front().buy_order_id, OrderId{401});
}

TEST(MatchingEngineTest, AddOrderCanProduceMultipleExecutionsThroughOrderBook)
{
    MatchingEngine engine;
    engine.register_market(btc_usdt());

    EXPECT_TRUE(engine.add_order(make_limit_order(btc_usdt(), OrderId{501}, vertex::core::UserId{21}, Side::Sell, 2, 100)).empty());
    EXPECT_TRUE(engine.add_order(make_limit_order(btc_usdt(), OrderId{502}, vertex::core::UserId{22}, Side::Sell, 3, 101)).empty());

    const auto executions = engine.add_order(make_limit_order(btc_usdt(), OrderId{503}, vertex::core::UserId{23}, Side::Buy, 5, 101));

    ASSERT_EQ(executions.size(), 2u);
    EXPECT_EQ(executions[0].sell_order_id, OrderId{501});
    EXPECT_EQ(executions[0].quantity, 2);
    EXPECT_EQ(executions[1].sell_order_id, OrderId{502});
    EXPECT_EQ(executions[1].quantity, 3);
    EXPECT_TRUE(executions[1].buy_fully_filled);
}

#if !defined(NDEBUG)
TEST(MatchingEngineDeathTest, AddOrderWithoutRegisteredMarketDies)
{
    ASSERT_DEATH(
        {
            MatchingEngine engine;
            (void)engine.add_order(make_limit_order(btc_usdt(), OrderId{900}, vertex::core::UserId{1}, Side::Buy, 1, 100));
        },
        ".*");
}

TEST(MatchingEngineDeathTest, CancelWithoutRegisteredMarketDies)
{
    ASSERT_DEATH(
        {
            MatchingEngine engine;
            (void)engine.cancel(btc_usdt(), OrderId{901});
        },
        ".*");
}

TEST(MatchingEngineDeathTest, RegisterDuplicateMarketDies)
{
    ASSERT_DEATH(
        {
            MatchingEngine engine;
            engine.register_market(btc_usdt());
            engine.register_market(btc_usdt());
        },
        ".*");
}
#endif
