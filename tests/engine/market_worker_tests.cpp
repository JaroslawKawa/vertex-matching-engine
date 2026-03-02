#include <gtest/gtest.h>

#include "vertex/engine/market_worker.hpp"

namespace
{
    using vertex::core::Asset;
    using vertex::core::Market;
    using vertex::core::OrderId;
    using vertex::core::Side;
    using vertex::core::UserId;
    using vertex::engine::EngineAsyncError;
    using vertex::engine::MarketWorker;
    using vertex::engine::OrderRequest;

    Market btc_usdt()
    {
        return Market{Asset{"btc"}, Asset{"usdt"}};
    }

    OrderRequest make_limit_order(
        OrderId order_id,
        UserId user_id,
        Side side,
        vertex::core::Quantity quantity,
        vertex::core::Price price)
    {
        return vertex::engine::LimitOrderRequest{
            .id = order_id,
            .user_id = user_id,
            .market = btc_usdt(),
            .side = side,
            .limit_price = price,
            .base_quantity = quantity,
        };
    }
} // namespace

TEST(MarketWorkerTest, ProcessesSubmitThenSubmitThenCancelInFIFOOrder)
{
    MarketWorker worker{btc_usdt()};

    auto first_submit_future = worker.submit(
        make_limit_order(OrderId{1}, UserId{10}, Side::Sell, 2, 100));
    auto second_submit_future = worker.submit(
        make_limit_order(OrderId{2}, UserId{11}, Side::Buy, 2, 100));
    auto cancel_future = worker.cancel(OrderId{1});

    auto first_submit = first_submit_future.get();
    ASSERT_TRUE(first_submit.has_value());
    EXPECT_TRUE(first_submit->empty());

    auto second_submit = second_submit_future.get();
    ASSERT_TRUE(second_submit.has_value());
    ASSERT_EQ(second_submit->size(), 1u);
    EXPECT_EQ((*second_submit)[0].buy_order_id, OrderId{2});
    EXPECT_EQ((*second_submit)[0].sell_order_id, OrderId{1});
    EXPECT_TRUE((*second_submit)[0].buy_fully_filled);
    EXPECT_TRUE((*second_submit)[0].sell_fully_filled);

    auto cancel_result = cancel_future.get();
    ASSERT_TRUE(cancel_result.has_value());
    EXPECT_EQ(*cancel_result, std::nullopt);
}

TEST(MarketWorkerTest, BestBidSeesAllEarlierSubmitTasks)
{
    MarketWorker worker{btc_usdt()};

    auto submit_a = worker.submit(
        make_limit_order(OrderId{101}, UserId{21}, Side::Buy, 1, 100));
    auto submit_b = worker.submit(
        make_limit_order(OrderId{102}, UserId{22}, Side::Buy, 1, 105));
    auto best_bid_future = worker.best_bid();

    ASSERT_TRUE(submit_a.get().has_value());
    ASSERT_TRUE(submit_b.get().has_value());

    auto best_bid = best_bid_future.get();
    ASSERT_TRUE(best_bid.has_value());
    ASSERT_TRUE(best_bid->has_value());
    EXPECT_EQ(best_bid->value(), 105);
}

TEST(MarketWorkerTest, StopMakesPublicApisReturnWorkerStopped)
{
    MarketWorker worker{btc_usdt()};
    worker.stop();

    auto submit_result = worker.submit(
        make_limit_order(OrderId{200}, UserId{31}, Side::Buy, 1, 100)).get();
    ASSERT_FALSE(submit_result.has_value());
    EXPECT_EQ(submit_result.error(), EngineAsyncError::WorkerStopped);

    auto cancel_result = worker.cancel(OrderId{200}).get();
    ASSERT_FALSE(cancel_result.has_value());
    EXPECT_EQ(cancel_result.error(), EngineAsyncError::WorkerStopped);

    auto best_bid_result = worker.best_bid().get();
    ASSERT_FALSE(best_bid_result.has_value());
    EXPECT_EQ(best_bid_result.error(), EngineAsyncError::WorkerStopped);

    auto best_ask_result = worker.best_ask().get();
    ASSERT_FALSE(best_ask_result.has_value());
    EXPECT_EQ(best_ask_result.error(), EngineAsyncError::WorkerStopped);
}
