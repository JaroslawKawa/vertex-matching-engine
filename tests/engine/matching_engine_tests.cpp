#include <gtest/gtest.h>

#include <memory>

#include "vertex/domain/limit_order.hpp"
#include "vertex/domain/market_order.hpp"
#include "vertex/engine/matching_engine.hpp"

namespace
{
    using vertex::core::Asset;
    using vertex::core::Market;
    using vertex::core::OrderId;
    using vertex::core::Side;
    using vertex::domain::LimitOrder;
    using vertex::domain::MarketOrder;
    using vertex::engine::MatchingEngine;

    Market btc_usdt()
    {
        return Market{Asset{"btc"}, Asset{"usdt"}};
    }

    Market eth_usdt()
    {
        return Market{Asset{"eth"}, Asset{"usdt"}};
    }

    std::unique_ptr<LimitOrder> make_limit_order(
        const Market &market,
        OrderId order_id,
        vertex::core::UserId user_id,
        Side side,
        vertex::core::Quantity quantity,
        vertex::core::Price price)
    {
        return std::make_unique<LimitOrder>(order_id, user_id, market, side, quantity, price);
    }

    std::unique_ptr<MarketOrder> make_market_order(
        const Market &market,
        OrderId order_id,
        vertex::core::UserId user_id,
        Side side,
        vertex::core::Quantity quantity)
    {
        return std::make_unique<MarketOrder>(order_id, user_id, market, side, quantity);
    }
}

TEST(MatchingEngineTest, RegisterMarketUpdatesPresence)
{
    MatchingEngine engine;

    EXPECT_FALSE(engine.has_market(btc_usdt()));
    engine.register_market(btc_usdt());
    EXPECT_TRUE(engine.has_market(btc_usdt()));
}

TEST(MatchingEngineTest, AddLimitOrderRoutesToRegisteredOrderBook)
{
    MatchingEngine engine;
    engine.register_market(btc_usdt());

    EXPECT_TRUE(engine.add_limit_order(make_limit_order(btc_usdt(), OrderId{100}, vertex::core::UserId{1}, Side::Sell, 5, 100)).empty());
    const auto executions = engine.add_limit_order(make_limit_order(btc_usdt(), OrderId{101}, vertex::core::UserId{2}, Side::Buy, 5, 110));

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

    EXPECT_TRUE(engine.add_limit_order(make_limit_order(btc_usdt(), OrderId{200}, vertex::core::UserId{7}, Side::Buy, 8, 99)).empty());
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

    EXPECT_TRUE(engine.add_limit_order(make_limit_order(btc_usdt(), OrderId{301}, vertex::core::UserId{11}, Side::Sell, 2, 100)).empty());
    EXPECT_TRUE(engine.add_limit_order(make_limit_order(eth_usdt(), OrderId{401}, vertex::core::UserId{12}, Side::Buy, 2, 1000)).empty());

    const auto btc_exec = engine.add_limit_order(make_limit_order(btc_usdt(), OrderId{302}, vertex::core::UserId{13}, Side::Buy, 2, 105));
    ASSERT_EQ(btc_exec.size(), 1u);
    EXPECT_EQ(btc_exec.front().sell_order_id, OrderId{301});

    const auto eth_exec = engine.add_limit_order(make_limit_order(eth_usdt(), OrderId{402}, vertex::core::UserId{14}, Side::Sell, 1, 995));
    ASSERT_EQ(eth_exec.size(), 1u);
    EXPECT_EQ(eth_exec.front().buy_order_id, OrderId{401});
}

TEST(MatchingEngineTest, AddLimitOrderCanProduceMultipleExecutionsThroughOrderBook)
{
    MatchingEngine engine;
    engine.register_market(btc_usdt());

    EXPECT_TRUE(engine.add_limit_order(make_limit_order(btc_usdt(), OrderId{501}, vertex::core::UserId{21}, Side::Sell, 2, 100)).empty());
    EXPECT_TRUE(engine.add_limit_order(make_limit_order(btc_usdt(), OrderId{502}, vertex::core::UserId{22}, Side::Sell, 3, 101)).empty());

    const auto executions = engine.add_limit_order(make_limit_order(btc_usdt(), OrderId{503}, vertex::core::UserId{23}, Side::Buy, 5, 101));

    ASSERT_EQ(executions.size(), 2u);
    EXPECT_EQ(executions[0].sell_order_id, OrderId{501});
    EXPECT_EQ(executions[0].quantity, 2);
    EXPECT_EQ(executions[1].sell_order_id, OrderId{502});
    EXPECT_EQ(executions[1].quantity, 3);
    EXPECT_TRUE(executions[1].buy_fully_filled);
}

TEST(MatchingEngineTest, ExecuteMarketOrderRoutesToRegisteredOrderBook)
{
    MatchingEngine engine;
    engine.register_market(btc_usdt());

    EXPECT_TRUE(engine.add_limit_order(make_limit_order(btc_usdt(), OrderId{550}, vertex::core::UserId{25}, Side::Sell, 2, 100)).empty());
    EXPECT_TRUE(engine.add_limit_order(make_limit_order(btc_usdt(), OrderId{551}, vertex::core::UserId{26}, Side::Sell, 3, 101)).empty());

    const auto executions = engine.execute_market_order(make_market_order(btc_usdt(), OrderId{552}, vertex::core::UserId{27}, Side::Buy, 4));

    ASSERT_EQ(executions.size(), 2u);
    EXPECT_EQ(executions[0].buy_order_id, OrderId{552});
    EXPECT_EQ(executions[0].sell_order_id, OrderId{550});
    EXPECT_EQ(executions[0].execution_price, 100);
    EXPECT_EQ(executions[0].quantity, 2);

    EXPECT_EQ(executions[1].buy_order_id, OrderId{552});
    EXPECT_EQ(executions[1].sell_order_id, OrderId{551});
    EXPECT_EQ(executions[1].execution_price, 101);
    EXPECT_EQ(executions[1].quantity, 2);
    EXPECT_TRUE(executions[1].buy_fully_filled);

    ASSERT_TRUE(engine.best_ask(btc_usdt()).has_value());
    EXPECT_EQ(*engine.best_ask(btc_usdt()), 101);
}

TEST(MatchingEngineTest, ExecuteMarketOrderWithoutLiquidityReturnsEmptyAndDoesNotChangeBook)
{
    MatchingEngine engine;
    engine.register_market(btc_usdt());

    const auto executions = engine.execute_market_order(make_market_order(btc_usdt(), OrderId{560}, vertex::core::UserId{28}, Side::Buy, 3));

    EXPECT_TRUE(executions.empty());
    EXPECT_EQ(engine.best_bid(btc_usdt()), std::nullopt);
    EXPECT_EQ(engine.best_ask(btc_usdt()), std::nullopt);
}

TEST(MatchingEngineTest, ExecuteMarketOrderIsIsolatedByMarket)
{
    MatchingEngine engine;
    engine.register_market(btc_usdt());
    engine.register_market(eth_usdt());

    EXPECT_TRUE(engine.add_limit_order(make_limit_order(btc_usdt(), OrderId{570}, vertex::core::UserId{29}, Side::Sell, 1, 100)).empty());
    EXPECT_TRUE(engine.add_limit_order(make_limit_order(eth_usdt(), OrderId{571}, vertex::core::UserId{30}, Side::Sell, 2, 1000)).empty());

    const auto btc_exec = engine.execute_market_order(make_market_order(btc_usdt(), OrderId{572}, vertex::core::UserId{31}, Side::Buy, 1));
    ASSERT_EQ(btc_exec.size(), 1u);
    EXPECT_EQ(btc_exec.front().sell_order_id, OrderId{570});

    EXPECT_EQ(engine.best_ask(btc_usdt()), std::nullopt);
    ASSERT_TRUE(engine.best_ask(eth_usdt()).has_value());
    EXPECT_EQ(*engine.best_ask(eth_usdt()), 1000);
}

TEST(MatchingEngineTest, BestBidAndAskAreEmptyForRegisteredMarketWithoutOrders)
{
    MatchingEngine engine;
    engine.register_market(btc_usdt());

    EXPECT_EQ(engine.best_bid(btc_usdt()), std::nullopt);
    EXPECT_EQ(engine.best_ask(btc_usdt()), std::nullopt);
}

TEST(MatchingEngineTest, BestBidAndAskReflectOrdersAndUpdateAfterCancel)
{
    MatchingEngine engine;
    engine.register_market(btc_usdt());

    EXPECT_TRUE(engine.add_limit_order(make_limit_order(btc_usdt(), OrderId{601}, vertex::core::UserId{31}, Side::Buy, 2, 99)).empty());
    EXPECT_TRUE(engine.add_limit_order(make_limit_order(btc_usdt(), OrderId{602}, vertex::core::UserId{32}, Side::Buy, 2, 101)).empty());
    EXPECT_TRUE(engine.add_limit_order(make_limit_order(btc_usdt(), OrderId{603}, vertex::core::UserId{33}, Side::Sell, 2, 105)).empty());
    EXPECT_TRUE(engine.add_limit_order(make_limit_order(btc_usdt(), OrderId{604}, vertex::core::UserId{34}, Side::Sell, 2, 103)).empty());

    ASSERT_TRUE(engine.best_bid(btc_usdt()).has_value());
    ASSERT_TRUE(engine.best_ask(btc_usdt()).has_value());
    EXPECT_EQ(*engine.best_bid(btc_usdt()), 101);
    EXPECT_EQ(*engine.best_ask(btc_usdt()), 103);

    ASSERT_TRUE(engine.cancel(btc_usdt(), OrderId{602}).has_value());
    ASSERT_TRUE(engine.cancel(btc_usdt(), OrderId{604}).has_value());

    ASSERT_TRUE(engine.best_bid(btc_usdt()).has_value());
    ASSERT_TRUE(engine.best_ask(btc_usdt()).has_value());
    EXPECT_EQ(*engine.best_bid(btc_usdt()), 99);
    EXPECT_EQ(*engine.best_ask(btc_usdt()), 105);
}

TEST(MatchingEngineTest, BestBidAndAskUpdateAfterMatchAndMarketsRemainIsolated)
{
    MatchingEngine engine;
    engine.register_market(btc_usdt());
    engine.register_market(eth_usdt());

    EXPECT_TRUE(engine.add_limit_order(make_limit_order(btc_usdt(), OrderId{701}, vertex::core::UserId{41}, Side::Sell, 3, 100)).empty());
    EXPECT_TRUE(engine.add_limit_order(make_limit_order(btc_usdt(), OrderId{702}, vertex::core::UserId{42}, Side::Buy, 2, 98)).empty());
    EXPECT_TRUE(engine.add_limit_order(make_limit_order(eth_usdt(), OrderId{703}, vertex::core::UserId{43}, Side::Buy, 1, 1000)).empty());

    EXPECT_EQ(*engine.best_ask(btc_usdt()), 100);
    EXPECT_EQ(*engine.best_bid(btc_usdt()), 98);
    EXPECT_EQ(*engine.best_bid(eth_usdt()), 1000);
    EXPECT_EQ(engine.best_ask(eth_usdt()), std::nullopt);

    const auto executions = engine.add_limit_order(make_limit_order(btc_usdt(), OrderId{704}, vertex::core::UserId{44}, Side::Buy, 3, 100));
    ASSERT_EQ(executions.size(), 1u);
    EXPECT_EQ(executions.front().quantity, 3);

    EXPECT_EQ(engine.best_ask(btc_usdt()), std::nullopt);
    ASSERT_TRUE(engine.best_bid(btc_usdt()).has_value());
    EXPECT_EQ(*engine.best_bid(btc_usdt()), 98);

    // ETH book should be unaffected by BTC matching.
    ASSERT_TRUE(engine.best_bid(eth_usdt()).has_value());
    EXPECT_EQ(*engine.best_bid(eth_usdt()), 1000);
    EXPECT_EQ(engine.best_ask(eth_usdt()), std::nullopt);
}

#if !defined(NDEBUG)
TEST(MatchingEngineDeathTest, AddLimitOrderWithoutRegisteredMarketDies)
{
    ASSERT_DEATH(
        {
            MatchingEngine engine;
            (void)engine.add_limit_order(make_limit_order(btc_usdt(), OrderId{900}, vertex::core::UserId{1}, Side::Buy, 1, 100));
        },
        ".*");
}

TEST(MatchingEngineDeathTest, ExecuteMarketOrderWithoutRegisteredMarketDies)
{
    ASSERT_DEATH(
        {
            MatchingEngine engine;
            (void)engine.execute_market_order(make_market_order(btc_usdt(), OrderId{905}, vertex::core::UserId{1}, Side::Buy, 1));
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

TEST(MatchingEngineDeathTest, BestBidWithoutRegisteredMarketDies)
{
    ASSERT_DEATH(
        {
            MatchingEngine engine;
            (void)engine.best_bid(btc_usdt());
        },
        ".*");
}

TEST(MatchingEngineDeathTest, BestAskWithoutRegisteredMarketDies)
{
    ASSERT_DEATH(
        {
            MatchingEngine engine;
            (void)engine.best_ask(btc_usdt());
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
