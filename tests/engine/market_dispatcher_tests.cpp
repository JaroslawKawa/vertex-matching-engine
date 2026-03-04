#include <gtest/gtest.h>

#include "vertex/engine/market_dispatcher.hpp"

namespace
{
    using vertex::core::Asset;
    using vertex::core::Market;
    using vertex::core::OrderId;
    using vertex::core::Side;
    using vertex::core::UserId;
    using vertex::engine::EngineAsyncError;
    using vertex::engine::MarketDispatcher;
    using vertex::engine::OrderRequest;

    Market btc_usdt()
    {
        return Market{Asset{"btc"}, Asset{"usdt"}};
    }

    Market eth_usdt()
    {
        return Market{Asset{"eth"}, Asset{"usdt"}};
    }

    OrderRequest make_limit_order(
        const Market &market,
        OrderId order_id,
        UserId user_id,
        Side side,
        vertex::core::Quantity quantity,
        vertex::core::Price price)
    {
        return vertex::engine::LimitOrderRequest{
            .id = order_id,
            .user_id = user_id,
            .market = market,
            .side = side,
            .limit_price = price,
            .base_quantity = quantity,
        };
    }
} // namespace

TEST(MarketDispatcherTest, RegisterMarketUpdatesPresence)
{
    MarketDispatcher dispatcher;

    EXPECT_FALSE(dispatcher.has_market(btc_usdt()));
    const auto register_result = dispatcher.register_market(btc_usdt());
    ASSERT_TRUE(register_result.has_value());
    EXPECT_TRUE(dispatcher.has_market(btc_usdt()));
}

TEST(MarketDispatcherTest, RegisterDuplicateMarketReturnsAlreadyRegistered)
{
    MarketDispatcher dispatcher;

    ASSERT_TRUE(dispatcher.register_market(btc_usdt()).has_value());
    const auto second = dispatcher.register_market(btc_usdt());

    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error(), EngineAsyncError::MarketAlreadyRegistered);
}

TEST(MarketDispatcherTest, SubmitUnknownMarketReturnsMarketNotFound)
{
    MarketDispatcher dispatcher;

    auto result = dispatcher.submit(
        make_limit_order(btc_usdt(), OrderId{100}, UserId{1}, Side::Buy, 1, 100))
                      .get();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), EngineAsyncError::MarketNotFound);
}

TEST(MarketDispatcherTest, CancelAndBestBidUnknownMarketReturnMarketNotFound)
{
    MarketDispatcher dispatcher;

    auto cancel_result = dispatcher.cancel(btc_usdt(), OrderId{1}).get();
    ASSERT_FALSE(cancel_result.has_value());
    EXPECT_EQ(cancel_result.error(), EngineAsyncError::MarketNotFound);

    auto best_bid_result = dispatcher.best_bid(btc_usdt()).get();
    ASSERT_FALSE(best_bid_result.has_value());
    EXPECT_EQ(best_bid_result.error(), EngineAsyncError::MarketNotFound);
}

TEST(MarketDispatcherTest, SubmitRoutesToCorrectMarketWorker)
{
    MarketDispatcher dispatcher;
    ASSERT_TRUE(dispatcher.register_market(btc_usdt()).has_value());
    ASSERT_TRUE(dispatcher.register_market(eth_usdt()).has_value());

    ASSERT_TRUE(dispatcher.submit(
                             make_limit_order(btc_usdt(), OrderId{10}, UserId{1}, Side::Sell, 2, 100))
                    .get()
                    .has_value());
    ASSERT_TRUE(dispatcher.submit(
                             make_limit_order(eth_usdt(), OrderId{20}, UserId{2}, Side::Sell, 2, 1000))
                    .get()
                    .has_value());

    auto btc_match = dispatcher.submit(
                               make_limit_order(btc_usdt(), OrderId{11}, UserId{3}, Side::Buy, 2, 100))
                             .get();

    ASSERT_TRUE(btc_match.has_value());
    ASSERT_EQ(btc_match->size(), 1u);
    EXPECT_EQ((*btc_match)[0].buy_order_id, OrderId{11});
    EXPECT_EQ((*btc_match)[0].sell_order_id, OrderId{10});
    EXPECT_EQ((*btc_match)[0].execution_price, 100);
}

TEST(MarketDispatcherTest, StopAllBlocksNewRegistrationsAndWorkerCalls)
{
    MarketDispatcher dispatcher;
    ASSERT_TRUE(dispatcher.register_market(btc_usdt()).has_value());

    dispatcher.stop_all();

    const auto register_after_stop = dispatcher.register_market(eth_usdt());
    ASSERT_FALSE(register_after_stop.has_value());
    EXPECT_EQ(register_after_stop.error(), EngineAsyncError::WorkerStopped);

    try
    {
        auto submit_future = dispatcher.submit(
            make_limit_order(btc_usdt(), OrderId{100}, UserId{1}, Side::Buy, 1, 100));
        ASSERT_TRUE(submit_future.valid());
        auto submit_after_stop = submit_future.get();
        ASSERT_FALSE(submit_after_stop.has_value());
        EXPECT_EQ(submit_after_stop.error(), EngineAsyncError::WorkerStopped);
    }
    catch (const std::exception &e)
    {
        FAIL() << "submit after stop threw: " << e.what();
    }

    try
    {
        auto best_ask_future = dispatcher.best_ask(btc_usdt());
        ASSERT_TRUE(best_ask_future.valid());
        auto best_ask_after_stop = best_ask_future.get();
        ASSERT_FALSE(best_ask_after_stop.has_value());
        EXPECT_EQ(best_ask_after_stop.error(), EngineAsyncError::WorkerStopped);
    }
    catch (const std::exception &e)
    {
        FAIL() << "best_ask after stop threw: " << e.what();
    }
}
