#include <gtest/gtest.h>

#include "vertex/application/exchange.hpp"

namespace
{
    using vertex::application::Exchange;
    using vertex::application::PlaceOrderError;
    using vertex::application::RegisterMarketError;
    using vertex::application::UserError;
    using vertex::application::WalletOperationError;
    using vertex::core::Asset;
    using vertex::core::Market;
    using vertex::core::Side;
    using vertex::core::UserId;

    Market btc_usdt()
    {
        return Market{Asset{"btc"}, Asset{"usdt"}};
    }
}

TEST(ExchangeTest, CreateUserAndReadName)
{
    Exchange exchange;

    const auto user_result = exchange.create_user("jarek");
    ASSERT_TRUE(user_result.has_value());
    const UserId user_id = *user_result;

    const auto name_result = exchange.get_user_name(user_id);
    ASSERT_TRUE(name_result.has_value());
    EXPECT_EQ(*name_result, "jarek");
    EXPECT_TRUE(exchange.user_exists(user_id));
}

TEST(ExchangeTest, CreateUserRejectsEmptyName)
{
    Exchange exchange;

    const auto result = exchange.create_user("");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), UserError::EmptyName);
}

TEST(ExchangeTest, DepositWithdrawReserveReleaseFlow)
{
    Exchange exchange;
    const auto user_result = exchange.create_user("alice");
    ASSERT_TRUE(user_result.has_value());
    const UserId user_id = *user_result;

    ASSERT_TRUE(exchange.deposit(user_id, Asset{"usdt"}, 100).has_value());
    ASSERT_TRUE(exchange.reserve(user_id, Asset{"usdt"}, 30).has_value());

    const auto free_after_reserve = exchange.free_balance(user_id, Asset{"usdt"});
    const auto reserved_after_reserve = exchange.reserved_balance(user_id, Asset{"usdt"});
    ASSERT_TRUE(free_after_reserve.has_value());
    ASSERT_TRUE(reserved_after_reserve.has_value());
    EXPECT_EQ(*free_after_reserve, 70);
    EXPECT_EQ(*reserved_after_reserve, 30);

    ASSERT_TRUE(exchange.release(user_id, Asset{"usdt"}, 10).has_value());
    ASSERT_TRUE(exchange.withdraw(user_id, Asset{"usdt"}, 20).has_value());

    const auto free_final = exchange.free_balance(user_id, Asset{"usdt"});
    const auto reserved_final = exchange.reserved_balance(user_id, Asset{"usdt"});
    ASSERT_TRUE(free_final.has_value());
    ASSERT_TRUE(reserved_final.has_value());
    EXPECT_EQ(*free_final, 60);
    EXPECT_EQ(*reserved_final, 20);
}

TEST(ExchangeTest, DepositForMissingUserReturnsUserNotFound)
{
    Exchange exchange;

    const auto result = exchange.deposit(UserId{999}, Asset{"btc"}, 10);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), WalletOperationError::UserNotFound);
}

TEST(ExchangeTest, RegisterMarketAllowsFirstThenRejectsDuplicate)
{
    Exchange exchange;

    const auto first = exchange.register_market(btc_usdt());
    const auto second = exchange.register_market(btc_usdt());

    ASSERT_TRUE(first.has_value());
    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error(), RegisterMarketError::AlreadyListed);
}

TEST(ExchangeTest, PlaceLimitOrderValidatesInputs)
{
    Exchange exchange;

    const auto bad_user = exchange.place_limit_order(UserId{}, btc_usdt(), Side::Buy, 100, 1);
    ASSERT_FALSE(bad_user.has_value());
    EXPECT_EQ(bad_user.error(), PlaceOrderError::UserNotFound);

    const auto user_result = exchange.create_user("bob");
    ASSERT_TRUE(user_result.has_value());
    const UserId user_id = *user_result;

    const auto unlisted = exchange.place_limit_order(user_id, btc_usdt(), Side::Buy, 100, 1);
    ASSERT_FALSE(unlisted.has_value());
    EXPECT_EQ(unlisted.error(), PlaceOrderError::MarketNotListed);

    ASSERT_TRUE(exchange.register_market(btc_usdt()).has_value());

    const auto bad_qty = exchange.place_limit_order(user_id, btc_usdt(), Side::Buy, 100, 0);
    ASSERT_FALSE(bad_qty.has_value());
    EXPECT_EQ(bad_qty.error(), PlaceOrderError::InvalidQuantity);

    const auto bad_price = exchange.place_limit_order(user_id, btc_usdt(), Side::Buy, 0, 1);
    ASSERT_FALSE(bad_price.has_value());
    EXPECT_EQ(bad_price.error(), PlaceOrderError::InvalidAmount);
}

TEST(ExchangeTest, PlaceLimitOrderRejectsWhenInsufficientFunds)
{
    Exchange exchange;
    const auto user_result = exchange.create_user("carol");
    ASSERT_TRUE(user_result.has_value());
    const UserId user_id = *user_result;
    ASSERT_TRUE(exchange.register_market(btc_usdt()).has_value());

    const auto result = exchange.place_limit_order(user_id, btc_usdt(), Side::Buy, 100, 1);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), PlaceOrderError::InsufficientFunds);
}

TEST(ExchangeTest, PlaceLimitOrderWithoutMatchReturnsOpenOrder)
{
    Exchange exchange;
    const auto user_result = exchange.create_user("dave");
    ASSERT_TRUE(user_result.has_value());
    const UserId user_id = *user_result;
    ASSERT_TRUE(exchange.register_market(btc_usdt()).has_value());
    ASSERT_TRUE(exchange.deposit(user_id, Asset{"usdt"}, 1000).has_value());

    const auto place_result = exchange.place_limit_order(user_id, btc_usdt(), Side::Buy, 100, 5);

    ASSERT_TRUE(place_result.has_value());
    EXPECT_TRUE(place_result->order_id.is_valid());
    EXPECT_EQ(place_result->filled_quantity, 0);
    EXPECT_EQ(place_result->remaining_quantity, 5);
}

TEST(ExchangeTest, MatchSettlementIsAppliedExactlyOnceWithPriceImprovement)
{
    Exchange exchange;
    ASSERT_TRUE(exchange.register_market(btc_usdt()).has_value());

    const auto buyer_result = exchange.create_user("buyer");
    const auto seller_result = exchange.create_user("seller");
    ASSERT_TRUE(buyer_result.has_value());
    ASSERT_TRUE(seller_result.has_value());
    const UserId buyer_id = *buyer_result;
    const UserId seller_id = *seller_result;

    ASSERT_TRUE(exchange.deposit(buyer_id, Asset{"usdt"}, 1000).has_value());
    ASSERT_TRUE(exchange.deposit(seller_id, Asset{"btc"}, 10).has_value());

    const auto resting_sell = exchange.place_limit_order(seller_id, btc_usdt(), Side::Sell, 100, 5);
    ASSERT_TRUE(resting_sell.has_value());
    EXPECT_EQ(resting_sell->filled_quantity, 0);
    EXPECT_EQ(resting_sell->remaining_quantity, 5);

    const auto taker_buy = exchange.place_limit_order(buyer_id, btc_usdt(), Side::Buy, 110, 5);
    ASSERT_TRUE(taker_buy.has_value());
    EXPECT_EQ(taker_buy->filled_quantity, 5);
    EXPECT_EQ(taker_buy->remaining_quantity, 0);

    const auto buyer_usdt_free = exchange.free_balance(buyer_id, Asset{"usdt"});
    const auto buyer_usdt_reserved = exchange.reserved_balance(buyer_id, Asset{"usdt"});
    const auto buyer_btc_free = exchange.free_balance(buyer_id, Asset{"btc"});
    ASSERT_TRUE(buyer_usdt_free.has_value());
    ASSERT_TRUE(buyer_usdt_reserved.has_value());
    ASSERT_TRUE(buyer_btc_free.has_value());
    EXPECT_EQ(*buyer_usdt_free, 500);
    EXPECT_EQ(*buyer_usdt_reserved, 0);
    EXPECT_EQ(*buyer_btc_free, 5);

    const auto seller_btc_free = exchange.free_balance(seller_id, Asset{"btc"});
    const auto seller_btc_reserved = exchange.reserved_balance(seller_id, Asset{"btc"});
    const auto seller_usdt_free = exchange.free_balance(seller_id, Asset{"usdt"});
    ASSERT_TRUE(seller_btc_free.has_value());
    ASSERT_TRUE(seller_btc_reserved.has_value());
    ASSERT_TRUE(seller_usdt_free.has_value());
    EXPECT_EQ(*seller_btc_free, 5);
    EXPECT_EQ(*seller_btc_reserved, 0);
    EXPECT_EQ(*seller_usdt_free, 500);
}
