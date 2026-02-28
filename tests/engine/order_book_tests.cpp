#include <gtest/gtest.h>

#include <memory>

#include "vertex/engine/order_book.hpp"

namespace
{
    using vertex::core::Asset;
    using vertex::core::Market;
    using vertex::core::OrderId;
    using vertex::core::Price;
    using vertex::core::Quantity;
    using vertex::core::Side;
    using vertex::engine::Execution;
    using vertex::engine::OrderBook;
    using vertex::engine::RestingOrder;

    Market btc_usdt()
    {
        return Market{Asset{"btc"}, Asset{"usdt"}};
    }

    std::vector<Execution> submit_limit_order(OrderBook &book, OrderId order_id, Side side, Quantity quantity, Price price)
    {
        Quantity remaining = quantity;
        std::vector<Execution> executions = side == Side::Buy
                                                ? book.match_limit_buy_against_asks(order_id, price, remaining)
                                                : book.match_limit_sell_against_bids(order_id, price, remaining);

        if (remaining > 0)
        {
            RestingOrder resting{
                .id = order_id,
                .limit_price = price,
                .initial_base_quantity = quantity,
                .remaining_base_quantity = remaining,
            };
            book.insert_resting(side, std::move(resting));
        }

        return executions;
    }

    std::vector<Execution> submit_market_buy_by_quote(OrderBook &book, OrderId order_id, Quantity quote_budget)
    {
        return book.match_market_buy_by_quote_against_asks(order_id, quote_budget);
    }

    std::vector<Execution> submit_market_sell_by_base(OrderBook &book, OrderId order_id, Quantity base_quantity)
    {
        return book.match_market_sell_by_base_against_bids(order_id, base_quantity);
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

    const auto executions = submit_limit_order(book, OrderId{1}, Side::Buy, 5, 101);

    EXPECT_TRUE(executions.empty());
    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(*book.best_bid(), 101);
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBookTest, AddSellWithoutMatchUpdatesBestAsk)
{
    OrderBook book{btc_usdt()};

    const auto executions = submit_limit_order(book, OrderId{2}, Side::Sell, 3, 110);

    EXPECT_TRUE(executions.empty());
    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(*book.best_ask(), 110);
    EXPECT_FALSE(book.best_bid().has_value());
}

TEST(OrderBookTest, IncomingBuyMatchesRestingSell)
{
    OrderBook book{btc_usdt()};
    EXPECT_TRUE(submit_limit_order(book, OrderId{11}, Side::Sell, 4, 100).empty());

    const auto executions = submit_limit_order(book, OrderId{12}, Side::Buy, 4, 105);

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
    EXPECT_TRUE(submit_limit_order(book, OrderId{21}, Side::Sell, 10, 200).empty());

    const auto executions = submit_limit_order(book, OrderId{22}, Side::Buy, 4, 210);

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
    EXPECT_TRUE(submit_limit_order(book, OrderId{31}, Side::Buy, 7, 123).empty());
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

    EXPECT_TRUE(submit_limit_order(book, OrderId{41}, Side::Buy, 3, 90).empty());
    const auto executions = submit_limit_order(book, OrderId{42}, Side::Sell, 2, 95);

    EXPECT_TRUE(executions.empty());
    ASSERT_TRUE(book.best_bid().has_value());
    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(*book.best_bid(), 90);
    EXPECT_EQ(*book.best_ask(), 95);
}

TEST(OrderBookTest, IncomingBuySweepsMultipleAskLevels)
{
    OrderBook book{btc_usdt()};
    EXPECT_TRUE(submit_limit_order(book, OrderId{51}, Side::Sell, 2, 100).empty());
    EXPECT_TRUE(submit_limit_order(book, OrderId{52}, Side::Sell, 3, 101).empty());
    EXPECT_TRUE(submit_limit_order(book, OrderId{53}, Side::Sell, 4, 102).empty());

    const auto executions = submit_limit_order(book, OrderId{54}, Side::Buy, 7, 102);

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
    EXPECT_TRUE(submit_limit_order(book, OrderId{61}, Side::Sell, 2, 100).empty());
    EXPECT_TRUE(submit_limit_order(book, OrderId{62}, Side::Sell, 2, 100).empty());

    const auto executions = submit_limit_order(book, OrderId{63}, Side::Buy, 3, 100);

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
    EXPECT_TRUE(submit_limit_order(book, OrderId{71}, Side::Buy, 5, 105).empty());

    const auto executions = submit_limit_order(book, OrderId{72}, Side::Sell, 3, 100);

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
    EXPECT_TRUE(submit_limit_order(book, OrderId{81}, Side::Buy, 1, 99).empty());
    EXPECT_TRUE(submit_limit_order(book, OrderId{82}, Side::Buy, 1, 101).empty());
    EXPECT_TRUE(submit_limit_order(book, OrderId{83}, Side::Buy, 1, 100).empty());

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

    const auto executions = submit_market_buy_by_quote(book, OrderId{90}, 5);

    EXPECT_TRUE(executions.empty());
    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
    EXPECT_FALSE(book.cancel(OrderId{90}).has_value());
}

TEST(OrderBookTest, ExecuteMarketBuyQuoteBudgetSweepsAsksAndLeavesOnlyRestingRemainder)
{
    OrderBook book{btc_usdt()};
    EXPECT_TRUE(submit_limit_order(book, OrderId{91}, Side::Sell, 2, 100).empty());
    EXPECT_TRUE(submit_limit_order(book, OrderId{92}, Side::Sell, 3, 101).empty());

    // BUY market quantity is quote budget here: 2*100 + 2*101 = 402
    const auto executions = submit_market_buy_by_quote(book, OrderId{93}, 402);

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
    EXPECT_TRUE(submit_limit_order(book, OrderId{94}, Side::Buy, 2, 105).empty());
    EXPECT_TRUE(submit_limit_order(book, OrderId{95}, Side::Buy, 1, 104).empty());

    const auto executions = submit_market_sell_by_base(book, OrderId{96}, 5);

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
