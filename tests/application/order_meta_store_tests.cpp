#include <gtest/gtest.h>

#include "vertex/application/order_meta_store.hpp"

namespace
{
    using vertex::application::OrderMeta;
    using vertex::application::OrderMetaStore;
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
}

TEST(OrderMetaStoreTest, TryInsertAndFindRoundTrip)
{
    OrderMetaStore store;

    const OrderId order_id{1};
    const OrderMeta meta{
        .owner = UserId{10},
        .market = btc_usdt(),
        .side = Side::Buy,
        .price = 100,
        .requested_base_qty = 5};

    EXPECT_TRUE(store.try_insert(order_id, meta));

    const auto found = store.find(order_id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->owner, UserId{10});
    EXPECT_EQ(found->market, btc_usdt());
    EXPECT_EQ(found->side, Side::Buy);
    EXPECT_EQ(found->price, 100);
    ASSERT_TRUE(found->requested_base_qty.has_value());
    EXPECT_EQ(*found->requested_base_qty, 5);
    EXPECT_EQ(found->executed_base_qty, 0);
    EXPECT_EQ(found->executed_quote_qty, 0);
    EXPECT_EQ(found->fill_count, 0);
    EXPECT_TRUE(found->trade_ids.empty());
}

TEST(OrderMetaStoreTest, TryInsertDuplicateIdReturnsFalse)
{
    OrderMetaStore store;
    const OrderId order_id{2};

    ASSERT_TRUE(store.try_insert(order_id, OrderMeta{
                                             .owner = UserId{11},
                                             .market = btc_usdt(),
                                             .side = Side::Sell,
                                             .price = 101,
                                             .requested_base_qty = 3}));

    EXPECT_FALSE(store.try_insert(order_id, OrderMeta{
                                                .owner = UserId{12},
                                                .market = btc_usdt(),
                                                .side = Side::Buy,
                                                .price = 102,
                                                .requested_base_qty = 7}));

    const auto found = store.find(order_id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->owner, UserId{11});
    EXPECT_EQ(found->side, Side::Sell);
    EXPECT_EQ(found->price, 101);
}

TEST(OrderMetaStoreTest, AppendFillUpdatesAggregatesAndTradeIds)
{
    OrderMetaStore store;
    const OrderId order_id{3};

    ASSERT_TRUE(store.try_insert(order_id, OrderMeta{
                                             .owner = UserId{13},
                                             .market = btc_usdt(),
                                             .side = Side::Buy,
                                             .price = 100,
                                             .requested_base_qty = 10}));

    EXPECT_TRUE(store.append_fill(order_id, TradeId{1001}, 2, 100));
    EXPECT_TRUE(store.append_fill(order_id, TradeId{1002}, 3, 101));

    const auto found = store.find(order_id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->executed_base_qty, 5);
    EXPECT_EQ(found->executed_quote_qty, 503);
    EXPECT_EQ(found->fill_count, 2);
    ASSERT_EQ(found->trade_ids.size(), 2U);
    EXPECT_EQ(found->trade_ids[0], TradeId{1001});
    EXPECT_EQ(found->trade_ids[1], TradeId{1002});
}

TEST(OrderMetaStoreTest, AppendFillMissingOrderReturnsFalse)
{
    OrderMetaStore store;

    EXPECT_FALSE(store.append_fill(OrderId{999}, TradeId{1}, 1, 100));
}

TEST(OrderMetaStoreTest, CloseAndExtractReturnsRecordAndErasesMeta)
{
    OrderMetaStore store;
    const OrderId order_id{4};

    ASSERT_TRUE(store.try_insert(order_id, OrderMeta{
                                             .owner = UserId{14},
                                             .market = btc_usdt(),
                                             .side = Side::Sell,
                                             .price = 105,
                                             .requested_base_qty = 8}));
    ASSERT_TRUE(store.append_fill(order_id, TradeId{2001}, 3, 104));
    ASSERT_TRUE(store.append_fill(order_id, TradeId{2002}, 2, 105));

    const auto extracted = store.close_and_extract(order_id, OrderStatus::Filled);
    ASSERT_TRUE(extracted.has_value());

    EXPECT_EQ(extracted->id, order_id);
    EXPECT_EQ(extracted->user_id, UserId{14});
    EXPECT_EQ(extracted->market, btc_usdt());
    EXPECT_EQ(extracted->side, Side::Sell);
    EXPECT_EQ(extracted->type, OrderType::LimitOrder);
    EXPECT_EQ(extracted->status, OrderStatus::Filled);
    ASSERT_TRUE(extracted->limit_price.has_value());
    EXPECT_EQ(*extracted->limit_price, 105);
    ASSERT_TRUE(extracted->requested_base_qty.has_value());
    EXPECT_EQ(*extracted->requested_base_qty, 8);
    EXPECT_FALSE(extracted->requested_quote_budget.has_value());
    EXPECT_EQ(extracted->executed_base_qty, 5);
    EXPECT_EQ(extracted->executed_quote_qty, 522);
    ASSERT_TRUE(extracted->avg_price.has_value());
    EXPECT_NEAR(*extracted->avg_price, 522.0 / 5.0, 1e-12);
    EXPECT_EQ(extracted->fill_count, 2);
    ASSERT_EQ(extracted->trade_ids.size(), 2U);
    EXPECT_EQ(extracted->trade_ids[0], TradeId{2001});
    EXPECT_EQ(extracted->trade_ids[1], TradeId{2002});

    EXPECT_FALSE(store.find(order_id).has_value());
    EXPECT_FALSE(store.close_and_extract(order_id, OrderStatus::Filled).has_value());
}

TEST(OrderMetaStoreTest, CloseAndExtractWithoutFillsHasNoAveragePrice)
{
    OrderMetaStore store;
    const OrderId order_id{5};

    ASSERT_TRUE(store.try_insert(order_id, OrderMeta{
                                             .owner = UserId{15},
                                             .market = btc_usdt(),
                                             .side = Side::Buy,
                                             .price = 99,
                                             .requested_base_qty = 4}));

    const auto extracted = store.close_and_extract(order_id, OrderStatus::Canceled);
    ASSERT_TRUE(extracted.has_value());
    EXPECT_EQ(extracted->status, OrderStatus::Canceled);
    EXPECT_EQ(extracted->executed_base_qty, 0);
    EXPECT_EQ(extracted->executed_quote_qty, 0);
    EXPECT_FALSE(extracted->avg_price.has_value());
    EXPECT_EQ(extracted->fill_count, 0);
    EXPECT_TRUE(extracted->trade_ids.empty());
}

TEST(OrderMetaStoreTest, EraseRemovesExistingOrder)
{
    OrderMetaStore store;
    const OrderId order_id{6};

    ASSERT_TRUE(store.try_insert(order_id, OrderMeta{
                                             .owner = UserId{16},
                                             .market = btc_usdt(),
                                             .side = Side::Sell,
                                             .price = 100,
                                             .requested_base_qty = 1}));

    EXPECT_TRUE(store.erase(order_id));
    EXPECT_FALSE(store.find(order_id).has_value());
    EXPECT_FALSE(store.erase(order_id));
}
