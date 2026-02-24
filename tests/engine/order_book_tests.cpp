#include <gtest/gtest.h>

#include <memory>

#include "vertex/domain/limit_order.hpp"
#include "vertex/engine/order_book.hpp"

namespace
{
    using vertex::core::Asset;
    using vertex::core::Market;
    using vertex::core::OrderId;
    using vertex::core::Side;
    using vertex::domain::LimitOrder;
    using vertex::engine::Order;
    using vertex::engine::OrderBook;

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

TEST(OrderBookTest, BestBidAndAskAreEmptyOnNewBook)
{
    OrderBook book{btc_usdt()};

    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBookTest, AddBuyWithoutMatchUpdatesBestBid)
{
    OrderBook book{btc_usdt()};

    const auto executions = book.add_order(make_limit_order(OrderId{1}, vertex::core::UserId{10}, Side::Buy, 5, 101));

    EXPECT_TRUE(executions.empty());
    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(*book.best_bid(), 101);
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBookTest, AddSellWithoutMatchUpdatesBestAsk)
{
    OrderBook book{btc_usdt()};

    const auto executions = book.add_order(make_limit_order(OrderId{2}, vertex::core::UserId{20}, Side::Sell, 3, 110));

    EXPECT_TRUE(executions.empty());
    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(*book.best_ask(), 110);
    EXPECT_FALSE(book.best_bid().has_value());
}

TEST(OrderBookTest, IncomingBuyMatchesRestingSell)
{
    OrderBook book{btc_usdt()};
    EXPECT_TRUE(book.add_order(make_limit_order(OrderId{11}, vertex::core::UserId{31}, Side::Sell, 4, 100)).empty());

    const auto executions = book.add_order(make_limit_order(OrderId{12}, vertex::core::UserId{32}, Side::Buy, 4, 105));

    ASSERT_EQ(executions.size(), 1u);
    const auto &execution = executions.front();
    EXPECT_EQ(execution.buy_order_id, OrderId{12});
    EXPECT_EQ(execution.sell_order_id, OrderId{11});
    EXPECT_EQ(execution.quantity, 4);
    EXPECT_EQ(execution.execution_price, 100);
    EXPECT_EQ(execution.buy_order_limit_price, 105);
    EXPECT_TRUE(execution.buy_fully_filled);
    EXPECT_TRUE(execution.sell_fully_filled);
    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBookTest, PartialMatchLeavesRestingRemainder)
{
    OrderBook book{btc_usdt()};
    EXPECT_TRUE(book.add_order(make_limit_order(OrderId{21}, vertex::core::UserId{41}, Side::Sell, 10, 200)).empty());

    const auto executions = book.add_order(make_limit_order(OrderId{22}, vertex::core::UserId{42}, Side::Buy, 4, 210));

    ASSERT_EQ(executions.size(), 1u);
    EXPECT_TRUE(executions.front().buy_fully_filled);
    EXPECT_FALSE(executions.front().sell_fully_filled);
    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(*book.best_ask(), 200);

    const auto cancel_result = book.cancel(OrderId{21});
    ASSERT_TRUE(cancel_result.has_value());
    EXPECT_EQ(cancel_result->remaining_quantity, 6);
}

TEST(OrderBookTest, CancelUnknownOrderReturnsNullopt)
{
    OrderBook book{btc_usdt()};

    EXPECT_FALSE(book.cancel(OrderId{999}).has_value());
}

TEST(OrderBookTest, CancelExistingOrderRemovesPriceLevel)
{
    OrderBook book{btc_usdt()};
    EXPECT_TRUE(book.add_order(make_limit_order(OrderId{31}, vertex::core::UserId{51}, Side::Buy, 7, 123)).empty());
    ASSERT_TRUE(book.best_bid().has_value());

    const auto cancel_result = book.cancel(OrderId{31});

    ASSERT_TRUE(cancel_result.has_value());
    EXPECT_EQ(cancel_result->id, OrderId{31});
    EXPECT_EQ(cancel_result->side, Side::Buy);
    EXPECT_EQ(cancel_result->price, 123);
    EXPECT_EQ(cancel_result->remaining_quantity, 7);
    EXPECT_FALSE(book.best_bid().has_value());
}
