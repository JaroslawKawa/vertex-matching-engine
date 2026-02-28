#include <gtest/gtest.h>

#include <memory>

#include "vertex/domain/limit_order.hpp"
#include "vertex/domain/market_order.hpp"
#include "vertex/engine/order_book.hpp"

namespace
{
    using vertex::core::Asset;
    using vertex::core::Market;
    using vertex::core::OrderId;
    using vertex::core::Side;
    using vertex::domain::LimitOrder;
    using vertex::domain::MarketOrder;
    using vertex::engine::Execution;
    using vertex::engine::OrderBook;
    using vertex::engine::RestingOrder;

    Market btc_usdt()
    {
        return Market{Asset{"btc"}, Asset{"usdt"}};
    }

    std::unique_ptr<LimitOrder> make_limit_order(
        OrderId order_id,
        vertex::core::UserId user_id,
        Side side,
        vertex::core::Quantity quantity,
        vertex::core::Price price)
    {
        return std::make_unique<LimitOrder>(order_id, user_id, btc_usdt(), side, quantity, price);
    }

    std::unique_ptr<MarketOrder> make_market_order(
        OrderId order_id,
        vertex::core::UserId user_id,
        Side side,
        vertex::core::Quantity quantity)
    {
        return std::make_unique<MarketOrder>(order_id, user_id, btc_usdt(), side, quantity);
    }

    std::vector<Execution> submit_limit_order(OrderBook &book, std::unique_ptr<LimitOrder> order)
    {
        vertex::core::Quantity remaining = order->remaining_quantity();
        std::vector<Execution> executions = order->side() == Side::Buy
                                                ? book.match_limit_buy_against_asks(order->id(), order->price(), remaining)
                                                : book.match_limit_sell_against_bids(order->id(), order->price(), remaining);

        if (remaining > 0)
        {
            RestingOrder resting{
                .order_id = order->id(),
                .limit_price = order->price(),
                .initial_base_quantity = order->initial_quantity(),
                .remaining_base_quantity = remaining,
            };
            book.insert_resting(order->side(), std::move(resting));
        }

        return executions;
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

    const auto executions = submit_limit_order(book, make_limit_order(OrderId{1}, vertex::core::UserId{10}, Side::Buy, 5, 101));

    EXPECT_TRUE(executions.empty());
    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(*book.best_bid(), 101);
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBookTest, AddSellWithoutMatchUpdatesBestAsk)
{
    OrderBook book{btc_usdt()};

    const auto executions = submit_limit_order(book, make_limit_order(OrderId{2}, vertex::core::UserId{20}, Side::Sell, 3, 110));

    EXPECT_TRUE(executions.empty());
    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(*book.best_ask(), 110);
    EXPECT_FALSE(book.best_bid().has_value());
}

TEST(OrderBookTest, IncomingBuyMatchesRestingSell)
{
    OrderBook book{btc_usdt()};
    EXPECT_TRUE(submit_limit_order(book, make_limit_order(OrderId{11}, vertex::core::UserId{31}, Side::Sell, 4, 100)).empty());

    const auto executions = submit_limit_order(book, make_limit_order(OrderId{12}, vertex::core::UserId{32}, Side::Buy, 4, 105));

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
    EXPECT_TRUE(submit_limit_order(book, make_limit_order(OrderId{21}, vertex::core::UserId{41}, Side::Sell, 10, 200)).empty());

    const auto executions = submit_limit_order(book, make_limit_order(OrderId{22}, vertex::core::UserId{42}, Side::Buy, 4, 210));

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
    EXPECT_TRUE(submit_limit_order(book, make_limit_order(OrderId{31}, vertex::core::UserId{51}, Side::Buy, 7, 123)).empty());
    ASSERT_TRUE(book.best_bid().has_value());

    const auto cancel_result = book.cancel(OrderId{31});

    ASSERT_TRUE(cancel_result.has_value());
    EXPECT_EQ(cancel_result->id, OrderId{31});
    EXPECT_EQ(cancel_result->side, Side::Buy);
    EXPECT_EQ(cancel_result->price, 123);
    EXPECT_EQ(cancel_result->remaining_quantity, 7);
    EXPECT_FALSE(book.best_bid().has_value());
}

TEST(OrderBookTest, NonCrossingOrdersDoNotExecute)
{
    OrderBook book{btc_usdt()};

    EXPECT_TRUE(submit_limit_order(book, make_limit_order(OrderId{41}, vertex::core::UserId{61}, Side::Buy, 3, 90)).empty());
    const auto executions = submit_limit_order(book, make_limit_order(OrderId{42}, vertex::core::UserId{62}, Side::Sell, 2, 95));

    EXPECT_TRUE(executions.empty());
    ASSERT_TRUE(book.best_bid().has_value());
    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(*book.best_bid(), 90);
    EXPECT_EQ(*book.best_ask(), 95);
}

TEST(OrderBookTest, IncomingBuySweepsMultipleAskLevels)
{
    OrderBook book{btc_usdt()};
    EXPECT_TRUE(submit_limit_order(book, make_limit_order(OrderId{51}, vertex::core::UserId{71}, Side::Sell, 2, 100)).empty());
    EXPECT_TRUE(submit_limit_order(book, make_limit_order(OrderId{52}, vertex::core::UserId{72}, Side::Sell, 3, 101)).empty());
    EXPECT_TRUE(submit_limit_order(book, make_limit_order(OrderId{53}, vertex::core::UserId{73}, Side::Sell, 4, 102)).empty());

    const auto executions = submit_limit_order(book, make_limit_order(OrderId{54}, vertex::core::UserId{74}, Side::Buy, 7, 102));

    ASSERT_EQ(executions.size(), 3u);
    EXPECT_EQ(executions[0].sell_order_id, OrderId{51});
    EXPECT_EQ(executions[0].execution_price, 100);
    EXPECT_EQ(executions[0].quantity, 2);

    EXPECT_EQ(executions[1].sell_order_id, OrderId{52});
    EXPECT_EQ(executions[1].execution_price, 101);
    EXPECT_EQ(executions[1].quantity, 3);

    EXPECT_EQ(executions[2].sell_order_id, OrderId{53});
    EXPECT_EQ(executions[2].execution_price, 102);
    EXPECT_EQ(executions[2].quantity, 2);
    EXPECT_TRUE(executions[2].buy_fully_filled);
    EXPECT_FALSE(executions[2].sell_fully_filled);

    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(*book.best_ask(), 102);
    const auto cancel_tail = book.cancel(OrderId{53});
    ASSERT_TRUE(cancel_tail.has_value());
    EXPECT_EQ(cancel_tail->remaining_quantity, 2);
}

TEST(OrderBookTest, FIFOIsRespectedWithinSamePriceLevel)
{
    OrderBook book{btc_usdt()};
    EXPECT_TRUE(submit_limit_order(book, make_limit_order(OrderId{61}, vertex::core::UserId{81}, Side::Sell, 2, 100)).empty());
    EXPECT_TRUE(submit_limit_order(book, make_limit_order(OrderId{62}, vertex::core::UserId{82}, Side::Sell, 2, 100)).empty());

    const auto executions = submit_limit_order(book, make_limit_order(OrderId{63}, vertex::core::UserId{83}, Side::Buy, 3, 100));

    ASSERT_EQ(executions.size(), 2u);
    EXPECT_EQ(executions[0].sell_order_id, OrderId{61});
    EXPECT_EQ(executions[0].quantity, 2);
    EXPECT_TRUE(executions[0].sell_fully_filled);

    EXPECT_EQ(executions[1].sell_order_id, OrderId{62});
    EXPECT_EQ(executions[1].quantity, 1);
    EXPECT_FALSE(executions[1].sell_fully_filled);

    const auto cancel_second = book.cancel(OrderId{62});
    ASSERT_TRUE(cancel_second.has_value());
    EXPECT_EQ(cancel_second->remaining_quantity, 1);
}

TEST(OrderBookTest, SellInitiatedExecutionCarriesRestingBuyLimitPrice)
{
    OrderBook book{btc_usdt()};
    EXPECT_TRUE(submit_limit_order(book, make_limit_order(OrderId{71}, vertex::core::UserId{91}, Side::Buy, 5, 105)).empty());

    const auto executions = submit_limit_order(book, make_limit_order(OrderId{72}, vertex::core::UserId{92}, Side::Sell, 3, 100));

    ASSERT_EQ(executions.size(), 1u);
    EXPECT_EQ(executions.front().buy_order_id, OrderId{71});
    EXPECT_EQ(executions.front().sell_order_id, OrderId{72});
    EXPECT_EQ(executions.front().execution_price, 105);
    EXPECT_EQ(executions.front().buy_order_limit_price, 105);
    EXPECT_TRUE(executions.front().sell_fully_filled);
    EXPECT_FALSE(executions.front().buy_fully_filled);
}

TEST(OrderBookTest, CancelRemovesOnlySelectedOrderAndKeepsOtherLevels)
{
    OrderBook book{btc_usdt()};
    EXPECT_TRUE(submit_limit_order(book, make_limit_order(OrderId{81}, vertex::core::UserId{101}, Side::Buy, 1, 99)).empty());
    EXPECT_TRUE(submit_limit_order(book, make_limit_order(OrderId{82}, vertex::core::UserId{102}, Side::Buy, 1, 101)).empty());
    EXPECT_TRUE(submit_limit_order(book, make_limit_order(OrderId{83}, vertex::core::UserId{103}, Side::Buy, 1, 100)).empty());

    const auto cancel_mid = book.cancel(OrderId{83});
    ASSERT_TRUE(cancel_mid.has_value());
    EXPECT_EQ(cancel_mid->price, 100);

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(*book.best_bid(), 101);
    EXPECT_FALSE(book.cancel(OrderId{83}).has_value());
}

TEST(OrderBookTest, ExecuteMarketBuyWithoutLiquidityReturnsNoExecutionsAndDoesNotInsertOrder)
{
    OrderBook book{btc_usdt()};

    const auto executions = book.execute_market_order(make_market_order(OrderId{90}, vertex::core::UserId{110}, Side::Buy, 5));

    EXPECT_TRUE(executions.empty());
    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
    EXPECT_FALSE(book.cancel(OrderId{90}).has_value());
}

TEST(OrderBookTest, ExecuteMarketBuyQuoteBudgetSweepsAsksAndLeavesOnlyRestingRemainder)
{
    OrderBook book{btc_usdt()};
    EXPECT_TRUE(submit_limit_order(book, make_limit_order(OrderId{91}, vertex::core::UserId{111}, Side::Sell, 2, 100)).empty());
    EXPECT_TRUE(submit_limit_order(book, make_limit_order(OrderId{92}, vertex::core::UserId{112}, Side::Sell, 3, 101)).empty());

    // BUY market quantity is quote budget here: 2*100 + 2*101 = 402
    const auto executions = book.execute_market_order(make_market_order(OrderId{93}, vertex::core::UserId{113}, Side::Buy, 402));

    ASSERT_EQ(executions.size(), 2u);
    EXPECT_EQ(executions[0].buy_order_id, OrderId{93});
    EXPECT_EQ(executions[0].sell_order_id, OrderId{91});
    EXPECT_EQ(executions[0].execution_price, 100);
    EXPECT_EQ(executions[0].buy_order_limit_price, 100);
    EXPECT_EQ(executions[0].quantity, 2);
    EXPECT_FALSE(executions[0].buy_fully_filled);
    EXPECT_TRUE(executions[0].sell_fully_filled);

    EXPECT_EQ(executions[1].buy_order_id, OrderId{93});
    EXPECT_EQ(executions[1].sell_order_id, OrderId{92});
    EXPECT_EQ(executions[1].execution_price, 101);
    EXPECT_EQ(executions[1].buy_order_limit_price, 101);
    EXPECT_EQ(executions[1].quantity, 2);
    EXPECT_TRUE(executions[1].buy_fully_filled);
    EXPECT_FALSE(executions[1].sell_fully_filled);

    EXPECT_FALSE(book.best_bid().has_value());
    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(*book.best_ask(), 101);

    const auto tail = book.cancel(OrderId{92});
    ASSERT_TRUE(tail.has_value());
    EXPECT_EQ(tail->remaining_quantity, 1);
}

TEST(OrderBookTest, ExecuteMarketSellConsumesBidsAndUnfilledRemainderIsDropped)
{
    OrderBook book{btc_usdt()};
    EXPECT_TRUE(submit_limit_order(book, make_limit_order(OrderId{94}, vertex::core::UserId{114}, Side::Buy, 2, 105)).empty());
    EXPECT_TRUE(submit_limit_order(book, make_limit_order(OrderId{95}, vertex::core::UserId{115}, Side::Buy, 1, 104)).empty());

    const auto executions = book.execute_market_order(make_market_order(OrderId{96}, vertex::core::UserId{116}, Side::Sell, 5));

    ASSERT_EQ(executions.size(), 2u);
    EXPECT_EQ(executions[0].buy_order_id, OrderId{94});
    EXPECT_EQ(executions[0].sell_order_id, OrderId{96});
    EXPECT_EQ(executions[0].execution_price, 105);
    EXPECT_EQ(executions[0].buy_order_limit_price, 105);
    EXPECT_EQ(executions[0].quantity, 2);
    EXPECT_TRUE(executions[0].buy_fully_filled);
    EXPECT_FALSE(executions[0].sell_fully_filled);

    EXPECT_EQ(executions[1].buy_order_id, OrderId{95});
    EXPECT_EQ(executions[1].sell_order_id, OrderId{96});
    EXPECT_EQ(executions[1].execution_price, 104);
    EXPECT_EQ(executions[1].buy_order_limit_price, 104);
    EXPECT_EQ(executions[1].quantity, 1);
    EXPECT_TRUE(executions[1].buy_fully_filled);
    EXPECT_FALSE(executions[1].sell_fully_filled);

    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
    EXPECT_FALSE(book.cancel(OrderId{96}).has_value());
}
