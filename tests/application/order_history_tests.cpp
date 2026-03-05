#include <algorithm>

#include <gtest/gtest.h>

#include "vertex/application/order_history.hpp"

namespace
{
    using vertex::application::OrderHistory;
    using vertex::application::OrderRecord;
    using vertex::application::OrderStatus;
    using vertex::application::OrderType;
    using vertex::core::Asset;
    using vertex::core::Market;
    using vertex::core::OrderId;
    using vertex::core::Side;
    using vertex::core::TradeId;
    using vertex::core::UserId;

    Market btc_usdt()
    {
        return Market{Asset{"btc"}, Asset{"usdt"}};
    }

    bool contains_order_id(const std::vector<OrderRecord> &records, OrderId id)
    {
        return std::any_of(records.begin(), records.end(), [id](const OrderRecord &r) {
            return r.id == id;
        });
    }
}

TEST(OrderHistoryTest, FindUnknownOrderReturnsNullopt)
{
    OrderHistory history;

    EXPECT_FALSE(history.find(OrderId{999}).has_value());
}

TEST(OrderHistoryTest, TryInsertAndFindByIdRoundTrip)
{
    OrderHistory history;
    const OrderId order_id{1};

    const OrderRecord record{
        .id = order_id,
        .user_id = UserId{10},
        .market = btc_usdt(),
        .side = Side::Buy,
        .type = OrderType::MarketOrder,
        .status = OrderStatus::Unfilled,
        .limit_price = std::nullopt,
        .requested_base_qty = std::nullopt,
        .requested_quote_budget = 250,
        .executed_base_qty = 0,
        .executed_quote_qty = 0,
        .avg_price = std::nullopt,
        .fill_count = 0,
        .trade_ids = {}};

    ASSERT_TRUE(history.try_insert(record));

    const auto found = history.find(order_id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->id, order_id);
    EXPECT_EQ(found->user_id, UserId{10});
    EXPECT_EQ(found->market, btc_usdt());
    EXPECT_EQ(found->side, Side::Buy);
    EXPECT_EQ(found->type, OrderType::MarketOrder);
    EXPECT_EQ(found->status, OrderStatus::Unfilled);
    ASSERT_TRUE(found->requested_quote_budget.has_value());
    EXPECT_EQ(*found->requested_quote_budget, 250);
    EXPECT_FALSE(found->requested_base_qty.has_value());
    EXPECT_FALSE(found->avg_price.has_value());
    EXPECT_EQ(found->fill_count, 0);
    EXPECT_TRUE(found->trade_ids.empty());
}

TEST(OrderHistoryTest, DuplicateOrderIdReturnsFalse)
{
    OrderHistory history;
    const OrderId order_id{2};

    ASSERT_TRUE(history.try_insert(OrderRecord{
        .id = order_id,
        .user_id = UserId{20},
        .market = btc_usdt(),
        .side = Side::Sell,
        .type = OrderType::LimitOrder,
        .status = OrderStatus::Canceled,
        .limit_price = 101,
        .requested_base_qty = 3}));

    EXPECT_FALSE(history.try_insert(OrderRecord{
        .id = order_id,
        .user_id = UserId{21},
        .market = btc_usdt(),
        .side = Side::Buy,
        .type = OrderType::LimitOrder,
        .status = OrderStatus::Filled,
        .limit_price = 102,
        .requested_base_qty = 4}));

    const auto found = history.find(order_id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->user_id, UserId{20});
    EXPECT_EQ(found->side, Side::Sell);
    EXPECT_EQ(found->status, OrderStatus::Canceled);
}

TEST(OrderHistoryTest, FindByUserReturnsOnlyOrdersForThatUser)
{
    OrderHistory history;

    const OrderId user_order_1{10};
    const OrderId user_order_2{11};
    const OrderId other_user_order{12};
    const UserId user_id{100};

    ASSERT_TRUE(history.try_insert(OrderRecord{
        .id = user_order_1,
        .user_id = user_id,
        .market = btc_usdt(),
        .side = Side::Buy,
        .type = OrderType::MarketOrder,
        .status = OrderStatus::PartiallyFilled,
        .requested_quote_budget = 401,
        .executed_base_qty = 3,
        .executed_quote_qty = 301,
        .avg_price = 301.0 / 3.0,
        .fill_count = 2,
        .trade_ids = {TradeId{3001}, TradeId{3002}}}));

    ASSERT_TRUE(history.try_insert(OrderRecord{
        .id = user_order_2,
        .user_id = user_id,
        .market = btc_usdt(),
        .side = Side::Sell,
        .type = OrderType::LimitOrder,
        .status = OrderStatus::Filled,
        .limit_price = 104,
        .requested_base_qty = 1,
        .executed_base_qty = 1,
        .executed_quote_qty = 104,
        .avg_price = 104.0,
        .fill_count = 1,
        .trade_ids = {TradeId{3003}}}));

    ASSERT_TRUE(history.try_insert(OrderRecord{
        .id = other_user_order,
        .user_id = UserId{101},
        .market = btc_usdt(),
        .side = Side::Buy,
        .type = OrderType::LimitOrder,
        .status = OrderStatus::Filled,
        .limit_price = 100,
        .requested_base_qty = 2}));

    const auto by_user = history.find_by_user(user_id);
    ASSERT_TRUE(by_user.has_value());
    ASSERT_EQ(by_user->size(), 2U);
    EXPECT_TRUE(contains_order_id(*by_user, user_order_1));
    EXPECT_TRUE(contains_order_id(*by_user, user_order_2));
    EXPECT_FALSE(contains_order_id(*by_user, other_user_order));
}

TEST(OrderHistoryTest, FindByUserUnknownReturnsNullopt)
{
    OrderHistory history;

    EXPECT_FALSE(history.find_by_user(UserId{777}).has_value());
}
