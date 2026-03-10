#include <gtest/gtest.h>

#include "vertex/application/exchange.hpp"

namespace
{
    using vertex::application::AnalyticsError;
    using vertex::application::Exchange;
    using vertex::application::OrderStatus;
    using vertex::core::Asset;
    using vertex::core::Market;
    using vertex::core::Side;
    using vertex::core::UserId;

    Market btc_usdt()
    {
        return Market{Asset{"btc"}, Asset{"usdt"}};
    }

    Market eth_usdt()
    {
        return Market{Asset{"eth"}, Asset{"usdt"}};
    }

    UserId create_user_or_fail(Exchange &exchange, const char *name)
    {
        const auto user = exchange.create_user(name);
        EXPECT_TRUE(user.has_value());
        return *user;
    }
}

TEST(ExchangeAnalyticsTest, ReturnsInvalidUserIdAndUserNotFound)
{
    Exchange exchange;

    const auto invalid = exchange.order_count_by_status(UserId{}, OrderStatus::Filled);
    ASSERT_FALSE(invalid.has_value());
    EXPECT_EQ(invalid.error(), AnalyticsError::InvalidUserId);

    const auto missing = exchange.order_count_by_status(UserId{999}, OrderStatus::Filled);
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error(), AnalyticsError::UserNotFound);
}

TEST(ExchangeAnalyticsTest, ReturnsNoDataWhenUserHasNoCompletedOrders)
{
    Exchange exchange;
    const UserId user_id = create_user_or_fail(exchange, "alice");

    const auto count = exchange.order_count_by_status(user_id, OrderStatus::Filled);
    ASSERT_FALSE(count.has_value());
    EXPECT_EQ(count.error(), AnalyticsError::NoData);

    const auto total_quote = exchange.total_executed_quote_by_user(user_id);
    ASSERT_FALSE(total_quote.has_value());
    EXPECT_EQ(total_quote.error(), AnalyticsError::NoData);
}

TEST(ExchangeAnalyticsTest, AggregatesCompletedOrdersAcrossLimitCancelAndMarketFlows)
{
    Exchange exchange;
    ASSERT_TRUE(exchange.register_market(btc_usdt()).has_value());
    ASSERT_TRUE(exchange.register_market(eth_usdt()).has_value());

    const UserId alice = create_user_or_fail(exchange, "alice");
    const UserId bob = create_user_or_fail(exchange, "bob");

    ASSERT_TRUE(exchange.deposit(alice, Asset{"usdt"}, 2000).has_value());
    ASSERT_TRUE(exchange.deposit(bob, Asset{"btc"}, 10).has_value());

    const auto resting_btc_sell = exchange.place_limit_order(bob, btc_usdt(), Side::Sell, 100, 2);
    ASSERT_TRUE(resting_btc_sell.has_value());

    const auto alice_limit_fill = exchange.place_limit_order(alice, btc_usdt(), Side::Buy, 110, 2);
    ASSERT_TRUE(alice_limit_fill.has_value());
    EXPECT_EQ(alice_limit_fill->filled_quantity, 2);

    const auto alice_eth_limit = exchange.place_limit_order(alice, eth_usdt(), Side::Buy, 50, 1);
    ASSERT_TRUE(alice_eth_limit.has_value());
    const auto alice_cancel = exchange.cancel_order(alice, alice_eth_limit->order_id);
    ASSERT_TRUE(alice_cancel.has_value());

    const auto resting_btc_sell_2 = exchange.place_limit_order(bob, btc_usdt(), Side::Sell, 105, 1);
    ASSERT_TRUE(resting_btc_sell_2.has_value());

    const auto alice_market_buy = exchange.execute_market_order(alice, btc_usdt(), Side::Buy, 120);
    ASSERT_TRUE(alice_market_buy.has_value());
    EXPECT_EQ(alice_market_buy->filled_quantity, 105);
    EXPECT_EQ(alice_market_buy->remaining_quantity, 15);

    const auto filled_count = exchange.order_count_by_status(alice, OrderStatus::Filled);
    const auto partial_count = exchange.order_count_by_status(alice, OrderStatus::PartiallyFilled);
    const auto canceled_count = exchange.order_count_by_status(alice, OrderStatus::Canceled);
    const auto unfilled_count = exchange.order_count_by_status(alice, OrderStatus::Unfilled);
    ASSERT_TRUE(filled_count.has_value());
    ASSERT_TRUE(partial_count.has_value());
    ASSERT_TRUE(canceled_count.has_value());
    ASSERT_TRUE(unfilled_count.has_value());
    EXPECT_EQ(*filled_count, 1U);
    EXPECT_EQ(*partial_count, 1U);
    EXPECT_EQ(*canceled_count, 1U);
    EXPECT_EQ(*unfilled_count, 0U);

    const auto buy_count = exchange.order_count_by_side(alice, Side::Buy);
    const auto sell_count = exchange.order_count_by_side(alice, Side::Sell);
    ASSERT_TRUE(buy_count.has_value());
    ASSERT_TRUE(sell_count.has_value());
    EXPECT_EQ(*buy_count, 3U);
    EXPECT_EQ(*sell_count, 0U);

    const auto total_base = exchange.total_executed_base_by_user(alice);
    const auto total_quote = exchange.total_executed_quote_by_user(alice);
    ASSERT_TRUE(total_base.has_value());
    ASSERT_TRUE(total_quote.has_value());
    EXPECT_EQ(*total_base, 3);
    EXPECT_EQ(*total_quote, 305);

    const auto avg_fill_count = exchange.average_fill_count_by_user(alice);
    const auto completion = exchange.completion_ratio_by_user(alice);
    ASSERT_TRUE(avg_fill_count.has_value());
    ASSERT_TRUE(completion.has_value());
    EXPECT_NEAR(*avg_fill_count, 2.0 / 3.0, 1e-12);
    EXPECT_NEAR(*completion, 1.0 / 3.0, 1e-12);

    const auto avg_notional = exchange.avg_order_notional_by_user(alice);
    const auto vwap = exchange.vwap_from_orders_by_user(alice);
    const auto median_notional = exchange.median_order_notional_by_user(alice);
    ASSERT_TRUE(avg_notional.has_value());
    ASSERT_TRUE(vwap.has_value());
    ASSERT_TRUE(median_notional.has_value());
    EXPECT_NEAR(*avg_notional, 305.0 / 3.0, 1e-12);
    EXPECT_NEAR(*vwap, 305.0 / 3.0, 1e-12);
    EXPECT_DOUBLE_EQ(*median_notional, 105.0);

    const auto top2 = exchange.top_n_by_executed_quote_by_user(alice, 2);
    ASSERT_TRUE(top2.has_value());
    ASSERT_EQ(top2->size(), 2U);
    EXPECT_EQ((*top2)[0].first, alice_limit_fill->order_id);
    EXPECT_EQ((*top2)[0].second, 200);
    EXPECT_EQ((*top2)[1].first, alice_market_buy->order_id);
    EXPECT_EQ((*top2)[1].second, 105);

    const auto quote_by_market = exchange.executed_quote_by_market_for_user(alice);
    ASSERT_TRUE(quote_by_market.has_value());
    EXPECT_EQ(quote_by_market->at(btc_usdt()), 305);
    EXPECT_EQ(quote_by_market->at(eth_usdt()), 0);

    const auto ranked_markets = exchange.rank_markets_by_volume_for_user(alice);
    ASSERT_TRUE(ranked_markets.has_value());
    ASSERT_EQ(ranked_markets->size(), 2U);
    EXPECT_EQ((*ranked_markets)[0].first, btc_usdt());
    EXPECT_EQ((*ranked_markets)[0].second, 305);
    EXPECT_EQ((*ranked_markets)[1].first, eth_usdt());
    EXPECT_EQ((*ranked_markets)[1].second, 0);

    const auto slippage = exchange.avg_slippage_bps_for_limits_by_user(alice);
    ASSERT_TRUE(slippage.has_value());
    EXPECT_NEAR(*slippage, 909.0909090909091, 1e-9);
}

TEST(ExchangeAnalyticsTest, ReturnsNoDataForOptionalMetricsWhenHistoryHasOnlyZeroExecutions)
{
    Exchange exchange;
    ASSERT_TRUE(exchange.register_market(btc_usdt()).has_value());

    const UserId alice = create_user_or_fail(exchange, "alice");
    ASSERT_TRUE(exchange.deposit(alice, Asset{"usdt"}, 500).has_value());

    const auto open = exchange.place_limit_order(alice, btc_usdt(), Side::Buy, 100, 1);
    ASSERT_TRUE(open.has_value());
    ASSERT_TRUE(exchange.cancel_order(alice, open->order_id).has_value());

    const auto avg_notional = exchange.avg_order_notional_by_user(alice);
    const auto vwap = exchange.vwap_from_orders_by_user(alice);
    const auto slippage = exchange.avg_slippage_bps_for_limits_by_user(alice);
    ASSERT_FALSE(avg_notional.has_value());
    ASSERT_FALSE(vwap.has_value());
    ASSERT_FALSE(slippage.has_value());
    EXPECT_EQ(avg_notional.error(), AnalyticsError::NoData);
    EXPECT_EQ(vwap.error(), AnalyticsError::NoData);
    EXPECT_EQ(slippage.error(), AnalyticsError::NoData);

    const auto avg_fill_count = exchange.average_fill_count_by_user(alice);
    const auto completion = exchange.completion_ratio_by_user(alice);
    ASSERT_TRUE(avg_fill_count.has_value());
    ASSERT_TRUE(completion.has_value());
    EXPECT_DOUBLE_EQ(*avg_fill_count, 0.0);
    EXPECT_DOUBLE_EQ(*completion, 0.0);
}
