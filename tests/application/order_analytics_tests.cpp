#include <gtest/gtest.h>

#include <optional>
#include <vector>

#include "vertex/application/order_analytics.hpp"

namespace
{
    using vertex::application::OrderRecord;
    using vertex::application::OrderStatus;
    using vertex::application::OrderType;
    using vertex::core::Asset;
    using vertex::core::Market;
    using vertex::core::OrderId;
    using vertex::core::Quantity;
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

    OrderRecord make_order(
        OrderId id,
        UserId user_id,
        Market market,
        Side side,
        OrderType type,
        OrderStatus status,
        Quantity executed_base_qty,
        Quantity executed_quote_qty,
        std::size_t fill_count = 0,
        std::optional<vertex::core::Price> limit_price = std::nullopt,
        std::optional<double> avg_price = std::nullopt)
    {
        return OrderRecord{
            .id = id,
            .user_id = user_id,
            .market = market,
            .side = side,
            .type = type,
            .status = status,
            .limit_price = limit_price,
            .executed_base_qty = executed_base_qty,
            .executed_quote_qty = executed_quote_qty,
            .avg_price = avg_price,
            .fill_count = fill_count};
    }
} // namespace

TEST(OrderAnalyticsTest, CountByStatusAndSideWorks)
{
    const std::vector<OrderRecord> orders{
        make_order(OrderId{1}, UserId{10}, btc_usdt(), Side::Buy, OrderType::LimitOrder, OrderStatus::Filled, 2, 200),
        make_order(OrderId{2}, UserId{10}, btc_usdt(), Side::Buy, OrderType::MarketOrder, OrderStatus::PartiallyFilled, 1, 101),
        make_order(OrderId{3}, UserId{11}, eth_usdt(), Side::Sell, OrderType::LimitOrder, OrderStatus::Unfilled, 0, 0),
        make_order(OrderId{4}, UserId{12}, eth_usdt(), Side::Sell, OrderType::LimitOrder, OrderStatus::Canceled, 0, 0)};

    EXPECT_EQ(vertex::application::count_by_status(orders, OrderStatus::Filled), 1U);
    EXPECT_EQ(vertex::application::count_by_status(orders, OrderStatus::Unfilled), 1U);
    EXPECT_EQ(vertex::application::count_by_side(orders, Side::Buy), 2U);
    EXPECT_EQ(vertex::application::count_by_side(orders, Side::Sell), 2U);
}

TEST(OrderAnalyticsTest, TotalsAverageAndVwapWork)
{
    const std::vector<OrderRecord> orders{
        make_order(OrderId{1}, UserId{10}, btc_usdt(), Side::Buy, OrderType::LimitOrder, OrderStatus::Filled, 2, 200, 1),
        make_order(OrderId{2}, UserId{11}, btc_usdt(), Side::Sell, OrderType::LimitOrder, OrderStatus::PartiallyFilled, 3, 330, 2)};

    EXPECT_EQ(vertex::application::total_executed_base(orders), 5);
    EXPECT_EQ(vertex::application::total_executed_quote(orders), 530);

    const auto avg_fill = vertex::application::average_fill_count(orders);
    ASSERT_TRUE(avg_fill.has_value());
    EXPECT_DOUBLE_EQ(*avg_fill, 1.5);

    const auto completion = vertex::application::completion_ratio(orders);
    ASSERT_TRUE(completion.has_value());
    EXPECT_DOUBLE_EQ(*completion, 0.5);

    const auto avg_notional = vertex::application::avg_order_notional(orders);
    ASSERT_TRUE(avg_notional.has_value());
    EXPECT_DOUBLE_EQ(*avg_notional, 265.0);

    const auto vwap = vertex::application::vwap_from_orders(orders);
    ASSERT_TRUE(vwap.has_value());
    EXPECT_DOUBLE_EQ(*vwap, 106.0);
}

TEST(OrderAnalyticsTest, EmptyInputOptionalsReturnNullopt)
{
    const std::vector<OrderRecord> orders{};

    EXPECT_FALSE(vertex::application::average_fill_count(orders).has_value());
    EXPECT_FALSE(vertex::application::completion_ratio(orders).has_value());
    EXPECT_FALSE(vertex::application::avg_order_notional(orders).has_value());
    EXPECT_FALSE(vertex::application::vwap_from_orders(orders).has_value());
    EXPECT_FALSE(vertex::application::median_order_notional(orders).has_value());
    EXPECT_FALSE(vertex::application::avg_slippage_bps_for_limits(orders).has_value());
}

TEST(OrderAnalyticsTest, MedianOrderNotionalForOddCount)
{
    const std::vector<OrderRecord> orders{
        make_order(OrderId{1}, UserId{10}, btc_usdt(), Side::Buy, OrderType::LimitOrder, OrderStatus::Filled, 1, 10),
        make_order(OrderId{2}, UserId{10}, btc_usdt(), Side::Buy, OrderType::LimitOrder, OrderStatus::Filled, 1, 30),
        make_order(OrderId{3}, UserId{10}, btc_usdt(), Side::Buy, OrderType::LimitOrder, OrderStatus::Filled, 1, 20)};

    const auto median = vertex::application::median_order_notional(orders);
    ASSERT_TRUE(median.has_value());
    EXPECT_DOUBLE_EQ(*median, 20.0);
}

TEST(OrderAnalyticsTest, MedianOrderNotionalForEvenCount)
{
    const std::vector<OrderRecord> orders{
        make_order(OrderId{1}, UserId{10}, btc_usdt(), Side::Buy, OrderType::LimitOrder, OrderStatus::Filled, 1, 10),
        make_order(OrderId{2}, UserId{10}, btc_usdt(), Side::Buy, OrderType::LimitOrder, OrderStatus::Filled, 1, 30),
        make_order(OrderId{3}, UserId{10}, btc_usdt(), Side::Buy, OrderType::LimitOrder, OrderStatus::Filled, 1, 20),
        make_order(OrderId{4}, UserId{10}, btc_usdt(), Side::Buy, OrderType::LimitOrder, OrderStatus::Filled, 1, 40)};

    const auto median = vertex::application::median_order_notional(orders);
    ASSERT_TRUE(median.has_value());
    EXPECT_DOUBLE_EQ(*median, 25.0);
}

TEST(OrderAnalyticsTest, TopNByExecutedQuoteReturnsSortedTopWithTieBreak)
{
    const std::vector<OrderRecord> orders{
        make_order(OrderId{3}, UserId{10}, btc_usdt(), Side::Buy, OrderType::LimitOrder, OrderStatus::Filled, 1, 300),
        make_order(OrderId{1}, UserId{10}, btc_usdt(), Side::Buy, OrderType::LimitOrder, OrderStatus::Filled, 1, 500),
        make_order(OrderId{2}, UserId{10}, btc_usdt(), Side::Buy, OrderType::LimitOrder, OrderStatus::Filled, 1, 500),
        make_order(OrderId{4}, UserId{10}, btc_usdt(), Side::Buy, OrderType::LimitOrder, OrderStatus::Filled, 1, 100)};

    const auto top = vertex::application::top_n_by_executed_quote(orders, 3);
    ASSERT_EQ(top.size(), 3U);
    EXPECT_EQ(top[0].first, OrderId{1});
    EXPECT_EQ(top[0].second, 500);
    EXPECT_EQ(top[1].first, OrderId{2});
    EXPECT_EQ(top[1].second, 500);
    EXPECT_EQ(top[2].first, OrderId{3});
    EXPECT_EQ(top[2].second, 300);
}

TEST(OrderAnalyticsTest, TopNByExecutedQuoteHandlesZeroAndOversizedN)
{
    const std::vector<OrderRecord> orders{
        make_order(OrderId{1}, UserId{10}, btc_usdt(), Side::Buy, OrderType::LimitOrder, OrderStatus::Filled, 1, 20),
        make_order(OrderId{2}, UserId{10}, btc_usdt(), Side::Buy, OrderType::LimitOrder, OrderStatus::Filled, 1, 10)};

    const auto none = vertex::application::top_n_by_executed_quote(orders, 0);
    EXPECT_TRUE(none.empty());

    const auto all = vertex::application::top_n_by_executed_quote(orders, 100);
    ASSERT_EQ(all.size(), 2U);
    EXPECT_EQ(all[0].first, OrderId{1});
    EXPECT_EQ(all[1].first, OrderId{2});
}

TEST(OrderAnalyticsTest, ExecutedQuoteByMarketAndRankingWork)
{
    const std::vector<OrderRecord> orders{
        make_order(OrderId{1}, UserId{10}, btc_usdt(), Side::Buy, OrderType::LimitOrder, OrderStatus::Filled, 1, 100),
        make_order(OrderId{2}, UserId{10}, btc_usdt(), Side::Sell, OrderType::LimitOrder, OrderStatus::Filled, 1, 200),
        make_order(OrderId{3}, UserId{11}, eth_usdt(), Side::Buy, OrderType::LimitOrder, OrderStatus::Filled, 1, 150)};

    const auto by_market = vertex::application::executed_quote_by_market(orders);
    ASSERT_EQ(by_market.size(), 2U);
    EXPECT_EQ(by_market.at(btc_usdt()), 300);
    EXPECT_EQ(by_market.at(eth_usdt()), 150);

    const auto ranked = vertex::application::rank_markets_by_volume(orders);
    ASSERT_EQ(ranked.size(), 2U);
    EXPECT_EQ(ranked[0].first, btc_usdt());
    EXPECT_EQ(ranked[0].second, 300);
    EXPECT_EQ(ranked[1].first, eth_usdt());
    EXPECT_EQ(ranked[1].second, 150);
}

TEST(OrderAnalyticsTest, AvgSlippageFiltersInputAndComputesValue)
{
    const std::vector<OrderRecord> orders{
        make_order(OrderId{1}, UserId{10}, btc_usdt(), Side::Buy, OrderType::LimitOrder, OrderStatus::Filled, 1, 100, 1, 100, 99.0),   // +100 bps
        make_order(OrderId{2}, UserId{11}, btc_usdt(), Side::Sell, OrderType::LimitOrder, OrderStatus::Filled, 1, 101, 1, 100, 101.0), // +100 bps
        make_order(OrderId{3}, UserId{12}, btc_usdt(), Side::Buy, OrderType::MarketOrder, OrderStatus::Filled, 1, 100, 1, 100, 99.0),   // filtered out
        make_order(OrderId{4}, UserId{13}, btc_usdt(), Side::Buy, OrderType::LimitOrder, OrderStatus::Filled, 1, 100, 1, 0, 100.0),     // filtered out
        make_order(OrderId{5}, UserId{14}, btc_usdt(), Side::Buy, OrderType::LimitOrder, OrderStatus::Filled, 1, 100, 1, 100, std::nullopt)}; // filtered out

    const auto slippage = vertex::application::avg_slippage_bps_for_limits(orders);
    ASSERT_TRUE(slippage.has_value());
    EXPECT_DOUBLE_EQ(*slippage, 100.0);
}

TEST(OrderAnalyticsTest, AvgSlippageReturnsNulloptWhenNoValidLimitOrders)
{
    const std::vector<OrderRecord> orders{
        make_order(OrderId{1}, UserId{10}, btc_usdt(), Side::Buy, OrderType::MarketOrder, OrderStatus::Filled, 1, 100, 1, 100, 100.0),
        make_order(OrderId{2}, UserId{11}, btc_usdt(), Side::Buy, OrderType::LimitOrder, OrderStatus::Filled, 1, 100, 1, std::nullopt, 100.0)};

    EXPECT_FALSE(vertex::application::avg_slippage_bps_for_limits(orders).has_value());
}
