#include <gtest/gtest.h>

#include "vertex/application/exchange.hpp"

namespace
{
    using vertex::application::Exchange;
    using vertex::application::CancelOrderError;
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

TEST(ExchangeTest, CancelBuyOrderReleasesReservedQuoteAndRemovesOrder)
{
    Exchange exchange;
    ASSERT_TRUE(exchange.register_market(btc_usdt()).has_value());

    const auto user_result = exchange.create_user("buyer");
    ASSERT_TRUE(user_result.has_value());
    const UserId user_id = *user_result;

    ASSERT_TRUE(exchange.deposit(user_id, Asset{"usdt"}, 1000).has_value());

    const auto place_result = exchange.place_limit_order(user_id, btc_usdt(), Side::Buy, 100, 5);
    ASSERT_TRUE(place_result.has_value());

    const auto free_before_cancel = exchange.free_balance(user_id, Asset{"usdt"});
    const auto reserved_before_cancel = exchange.reserved_balance(user_id, Asset{"usdt"});
    ASSERT_TRUE(free_before_cancel.has_value());
    ASSERT_TRUE(reserved_before_cancel.has_value());
    EXPECT_EQ(*free_before_cancel, 500);
    EXPECT_EQ(*reserved_before_cancel, 500);

    const auto cancel_result = exchange.cancel_order(user_id, place_result->order_id);
    ASSERT_TRUE(cancel_result.has_value());
    EXPECT_EQ(cancel_result->id, place_result->order_id);
    EXPECT_EQ(cancel_result->side, Side::Buy);
    EXPECT_EQ(cancel_result->remaining_quantity, 5);

    const auto free_after_cancel = exchange.free_balance(user_id, Asset{"usdt"});
    const auto reserved_after_cancel = exchange.reserved_balance(user_id, Asset{"usdt"});
    ASSERT_TRUE(free_after_cancel.has_value());
    ASSERT_TRUE(reserved_after_cancel.has_value());
    EXPECT_EQ(*free_after_cancel, 1000);
    EXPECT_EQ(*reserved_after_cancel, 0);

    const auto cancel_again = exchange.cancel_order(user_id, place_result->order_id);
    ASSERT_FALSE(cancel_again.has_value());
    EXPECT_EQ(cancel_again.error(), CancelOrderError::OrderNotFound);
}

TEST(ExchangeTest, CancelSellOrderReleasesReservedBase)
{
    Exchange exchange;
    ASSERT_TRUE(exchange.register_market(btc_usdt()).has_value());

    const auto user_result = exchange.create_user("seller");
    ASSERT_TRUE(user_result.has_value());
    const UserId user_id = *user_result;

    ASSERT_TRUE(exchange.deposit(user_id, Asset{"btc"}, 10).has_value());

    const auto place_result = exchange.place_limit_order(user_id, btc_usdt(), Side::Sell, 100, 4);
    ASSERT_TRUE(place_result.has_value());

    const auto free_before_cancel = exchange.free_balance(user_id, Asset{"btc"});
    const auto reserved_before_cancel = exchange.reserved_balance(user_id, Asset{"btc"});
    ASSERT_TRUE(free_before_cancel.has_value());
    ASSERT_TRUE(reserved_before_cancel.has_value());
    EXPECT_EQ(*free_before_cancel, 6);
    EXPECT_EQ(*reserved_before_cancel, 4);

    const auto cancel_result = exchange.cancel_order(user_id, place_result->order_id);
    ASSERT_TRUE(cancel_result.has_value());
    EXPECT_EQ(cancel_result->id, place_result->order_id);
    EXPECT_EQ(cancel_result->side, Side::Sell);
    EXPECT_EQ(cancel_result->remaining_quantity, 4);

    const auto free_after_cancel = exchange.free_balance(user_id, Asset{"btc"});
    const auto reserved_after_cancel = exchange.reserved_balance(user_id, Asset{"btc"});
    ASSERT_TRUE(free_after_cancel.has_value());
    ASSERT_TRUE(reserved_after_cancel.has_value());
    EXPECT_EQ(*free_after_cancel, 10);
    EXPECT_EQ(*reserved_after_cancel, 0);
}

TEST(ExchangeTest, CancelOrderRejectsNonOwnerAndKeepsReservation)
{
    Exchange exchange;
    ASSERT_TRUE(exchange.register_market(btc_usdt()).has_value());

    const auto owner_result = exchange.create_user("owner");
    const auto other_result = exchange.create_user("other");
    ASSERT_TRUE(owner_result.has_value());
    ASSERT_TRUE(other_result.has_value());
    const UserId owner_id = *owner_result;
    const UserId other_id = *other_result;

    ASSERT_TRUE(exchange.deposit(owner_id, Asset{"usdt"}, 1000).has_value());

    const auto place_result = exchange.place_limit_order(owner_id, btc_usdt(), Side::Buy, 100, 2);
    ASSERT_TRUE(place_result.has_value());

    const auto cancel_by_other = exchange.cancel_order(other_id, place_result->order_id);
    ASSERT_FALSE(cancel_by_other.has_value());
    EXPECT_EQ(cancel_by_other.error(), CancelOrderError::NotOrderOwner);

    const auto owner_free = exchange.free_balance(owner_id, Asset{"usdt"});
    const auto owner_reserved = exchange.reserved_balance(owner_id, Asset{"usdt"});
    ASSERT_TRUE(owner_free.has_value());
    ASSERT_TRUE(owner_reserved.has_value());
    EXPECT_EQ(*owner_free, 800);
    EXPECT_EQ(*owner_reserved, 200);

    const auto cancel_by_owner = exchange.cancel_order(owner_id, place_result->order_id);
    ASSERT_TRUE(cancel_by_owner.has_value());
    EXPECT_EQ(cancel_by_owner->remaining_quantity, 2);
}

TEST(ExchangeTest, CancelOrderReturnsUserOrOrderNotFound)
{
    Exchange exchange;
    ASSERT_TRUE(exchange.register_market(btc_usdt()).has_value());

    const auto user_result = exchange.create_user("user");
    ASSERT_TRUE(user_result.has_value());
    const UserId user_id = *user_result;

    const auto missing_user = exchange.cancel_order(UserId{999}, vertex::core::OrderId{1});
    ASSERT_FALSE(missing_user.has_value());
    EXPECT_EQ(missing_user.error(), CancelOrderError::UserNotFound);

    const auto missing_order = exchange.cancel_order(user_id, vertex::core::OrderId{999});
    ASSERT_FALSE(missing_order.has_value());
    EXPECT_EQ(missing_order.error(), CancelOrderError::OrderNotFound);
}

TEST(ExchangeTest, CancelAfterPartialFillReleasesOnlyRemainingReservation)
{
    Exchange exchange;
    ASSERT_TRUE(exchange.register_market(btc_usdt()).has_value());

    const auto seller_result = exchange.create_user("seller");
    const auto buyer_result = exchange.create_user("buyer");
    ASSERT_TRUE(seller_result.has_value());
    ASSERT_TRUE(buyer_result.has_value());
    const UserId seller_id = *seller_result;
    const UserId buyer_id = *buyer_result;

    ASSERT_TRUE(exchange.deposit(seller_id, Asset{"btc"}, 10).has_value());
    ASSERT_TRUE(exchange.deposit(buyer_id, Asset{"usdt"}, 1000).has_value());

    const auto resting_sell = exchange.place_limit_order(seller_id, btc_usdt(), Side::Sell, 100, 5);
    ASSERT_TRUE(resting_sell.has_value());

    const auto taker_buy = exchange.place_limit_order(buyer_id, btc_usdt(), Side::Buy, 110, 2);
    ASSERT_TRUE(taker_buy.has_value());
    EXPECT_EQ(taker_buy->filled_quantity, 2);
    EXPECT_EQ(taker_buy->remaining_quantity, 0);

    const auto seller_btc_free_before_cancel = exchange.free_balance(seller_id, Asset{"btc"});
    const auto seller_btc_reserved_before_cancel = exchange.reserved_balance(seller_id, Asset{"btc"});
    ASSERT_TRUE(seller_btc_free_before_cancel.has_value());
    ASSERT_TRUE(seller_btc_reserved_before_cancel.has_value());
    EXPECT_EQ(*seller_btc_free_before_cancel, 5);
    EXPECT_EQ(*seller_btc_reserved_before_cancel, 3);

    const auto cancel_result = exchange.cancel_order(seller_id, resting_sell->order_id);
    ASSERT_TRUE(cancel_result.has_value());
    EXPECT_EQ(cancel_result->side, Side::Sell);
    EXPECT_EQ(cancel_result->remaining_quantity, 3);

    const auto seller_btc_free_after_cancel = exchange.free_balance(seller_id, Asset{"btc"});
    const auto seller_btc_reserved_after_cancel = exchange.reserved_balance(seller_id, Asset{"btc"});
    ASSERT_TRUE(seller_btc_free_after_cancel.has_value());
    ASSERT_TRUE(seller_btc_reserved_after_cancel.has_value());
    EXPECT_EQ(*seller_btc_free_after_cancel, 8);
    EXPECT_EQ(*seller_btc_reserved_after_cancel, 0);
}

TEST(ExchangeTest, ExecuteMarketOrderWithoutLiquidityReleasesReservedFunds)
{
    Exchange exchange;
    ASSERT_TRUE(exchange.register_market(btc_usdt()).has_value());

    const auto buyer_result = exchange.create_user("buyer");
    ASSERT_TRUE(buyer_result.has_value());
    const UserId buyer_id = *buyer_result;

    ASSERT_TRUE(exchange.deposit(buyer_id, Asset{"usdt"}, 1000).has_value());

    const auto result = exchange.execute_market_order(buyer_id, btc_usdt(), Side::Buy, 250);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->order_id.is_valid());
    EXPECT_EQ(result->filled_quantity, 0);
    EXPECT_EQ(result->remaining_quantity, 250);

    const auto buyer_usdt_free = exchange.free_balance(buyer_id, Asset{"usdt"});
    const auto buyer_usdt_reserved = exchange.reserved_balance(buyer_id, Asset{"usdt"});
    const auto buyer_btc_free = exchange.free_balance(buyer_id, Asset{"btc"});
    ASSERT_TRUE(buyer_usdt_free.has_value());
    ASSERT_TRUE(buyer_usdt_reserved.has_value());
    ASSERT_TRUE(buyer_btc_free.has_value());
    EXPECT_EQ(*buyer_usdt_free, 1000);
    EXPECT_EQ(*buyer_usdt_reserved, 0);
    EXPECT_EQ(*buyer_btc_free, 0);

    const auto cancel_market_order = exchange.cancel_order(buyer_id, result->order_id);
    ASSERT_FALSE(cancel_market_order.has_value());
    EXPECT_EQ(cancel_market_order.error(), CancelOrderError::OrderNotFound);
}

TEST(ExchangeTest, ExecuteMarketBuyUsesQuoteBudgetAndLeavesTooSmallRemainderReleased)
{
    Exchange exchange;
    ASSERT_TRUE(exchange.register_market(btc_usdt()).has_value());

    const auto buyer_result = exchange.create_user("buyer");
    const auto seller1_result = exchange.create_user("seller1");
    const auto seller2_result = exchange.create_user("seller2");
    ASSERT_TRUE(buyer_result.has_value());
    ASSERT_TRUE(seller1_result.has_value());
    ASSERT_TRUE(seller2_result.has_value());
    const UserId buyer_id = *buyer_result;
    const UserId seller1_id = *seller1_result;
    const UserId seller2_id = *seller2_result;

    ASSERT_TRUE(exchange.deposit(buyer_id, Asset{"usdt"}, 1000).has_value());
    ASSERT_TRUE(exchange.deposit(seller1_id, Asset{"btc"}, 2).has_value());
    ASSERT_TRUE(exchange.deposit(seller2_id, Asset{"btc"}, 3).has_value());

    const auto resting_sell_1 = exchange.place_limit_order(seller1_id, btc_usdt(), Side::Sell, 100, 2);
    const auto resting_sell_2 = exchange.place_limit_order(seller2_id, btc_usdt(), Side::Sell, 101, 3);
    ASSERT_TRUE(resting_sell_1.has_value());
    ASSERT_TRUE(resting_sell_2.has_value());

    // Budget = 401 USDT -> buys 2 BTC @100 and 1 BTC @101, leaves 100 USDT budget remainder.
    const auto taker_buy = exchange.execute_market_order(buyer_id, btc_usdt(), Side::Buy, 401);
    ASSERT_TRUE(taker_buy.has_value());
    EXPECT_EQ(taker_buy->filled_quantity, 301);      // quote spent
    EXPECT_EQ(taker_buy->remaining_quantity, 100);   // quote budget left

    const auto buyer_usdt_free = exchange.free_balance(buyer_id, Asset{"usdt"});
    const auto buyer_usdt_reserved = exchange.reserved_balance(buyer_id, Asset{"usdt"});
    const auto buyer_btc_free = exchange.free_balance(buyer_id, Asset{"btc"});
    ASSERT_TRUE(buyer_usdt_free.has_value());
    ASSERT_TRUE(buyer_usdt_reserved.has_value());
    ASSERT_TRUE(buyer_btc_free.has_value());
    EXPECT_EQ(*buyer_usdt_free, 699);
    EXPECT_EQ(*buyer_usdt_reserved, 0);
    EXPECT_EQ(*buyer_btc_free, 3);

    const auto seller1_btc_free = exchange.free_balance(seller1_id, Asset{"btc"});
    const auto seller1_btc_reserved = exchange.reserved_balance(seller1_id, Asset{"btc"});
    const auto seller1_usdt_free = exchange.free_balance(seller1_id, Asset{"usdt"});
    ASSERT_TRUE(seller1_btc_free.has_value());
    ASSERT_TRUE(seller1_btc_reserved.has_value());
    ASSERT_TRUE(seller1_usdt_free.has_value());
    EXPECT_EQ(*seller1_btc_free, 0);
    EXPECT_EQ(*seller1_btc_reserved, 0);
    EXPECT_EQ(*seller1_usdt_free, 200);

    const auto seller2_btc_free = exchange.free_balance(seller2_id, Asset{"btc"});
    const auto seller2_btc_reserved = exchange.reserved_balance(seller2_id, Asset{"btc"});
    const auto seller2_usdt_free = exchange.free_balance(seller2_id, Asset{"usdt"});
    ASSERT_TRUE(seller2_btc_free.has_value());
    ASSERT_TRUE(seller2_btc_reserved.has_value());
    ASSERT_TRUE(seller2_usdt_free.has_value());
    EXPECT_EQ(*seller2_btc_free, 0);
    EXPECT_EQ(*seller2_btc_reserved, 2);
    EXPECT_EQ(*seller2_usdt_free, 101);

    const auto cancel_remaining = exchange.cancel_order(seller2_id, resting_sell_2->order_id);
    ASSERT_TRUE(cancel_remaining.has_value());
    EXPECT_EQ(cancel_remaining->side, Side::Sell);
    EXPECT_EQ(cancel_remaining->remaining_quantity, 2);
}

TEST(ExchangeTest, ExecuteMarketSellSettlesQuoteAndReleasesUnfilledBaseRemainder)
{
    Exchange exchange;
    ASSERT_TRUE(exchange.register_market(btc_usdt()).has_value());

    const auto seller_result = exchange.create_user("seller");
    const auto buyer1_result = exchange.create_user("buyer1");
    const auto buyer2_result = exchange.create_user("buyer2");
    ASSERT_TRUE(seller_result.has_value());
    ASSERT_TRUE(buyer1_result.has_value());
    ASSERT_TRUE(buyer2_result.has_value());
    const UserId seller_id = *seller_result;
    const UserId buyer1_id = *buyer1_result;
    const UserId buyer2_id = *buyer2_result;

    ASSERT_TRUE(exchange.deposit(seller_id, Asset{"btc"}, 5).has_value());
    ASSERT_TRUE(exchange.deposit(buyer1_id, Asset{"usdt"}, 1000).has_value());
    ASSERT_TRUE(exchange.deposit(buyer2_id, Asset{"usdt"}, 1000).has_value());

    const auto resting_buy_1 = exchange.place_limit_order(buyer1_id, btc_usdt(), Side::Buy, 105, 2);
    const auto resting_buy_2 = exchange.place_limit_order(buyer2_id, btc_usdt(), Side::Buy, 104, 1);
    ASSERT_TRUE(resting_buy_1.has_value());
    ASSERT_TRUE(resting_buy_2.has_value());

    const auto taker_sell = exchange.execute_market_order(seller_id, btc_usdt(), Side::Sell, 5);
    ASSERT_TRUE(taker_sell.has_value());
    EXPECT_EQ(taker_sell->filled_quantity, 3);     // base sold
    EXPECT_EQ(taker_sell->remaining_quantity, 2);  // base unsold, then released

    const auto seller_btc_free = exchange.free_balance(seller_id, Asset{"btc"});
    const auto seller_btc_reserved = exchange.reserved_balance(seller_id, Asset{"btc"});
    const auto seller_usdt_free = exchange.free_balance(seller_id, Asset{"usdt"});
    ASSERT_TRUE(seller_btc_free.has_value());
    ASSERT_TRUE(seller_btc_reserved.has_value());
    ASSERT_TRUE(seller_usdt_free.has_value());
    EXPECT_EQ(*seller_btc_free, 2);
    EXPECT_EQ(*seller_btc_reserved, 0);
    EXPECT_EQ(*seller_usdt_free, 314);

    const auto buyer1_usdt_free = exchange.free_balance(buyer1_id, Asset{"usdt"});
    const auto buyer1_usdt_reserved = exchange.reserved_balance(buyer1_id, Asset{"usdt"});
    const auto buyer1_btc_free = exchange.free_balance(buyer1_id, Asset{"btc"});
    ASSERT_TRUE(buyer1_usdt_free.has_value());
    ASSERT_TRUE(buyer1_usdt_reserved.has_value());
    ASSERT_TRUE(buyer1_btc_free.has_value());
    EXPECT_EQ(*buyer1_usdt_free, 790);
    EXPECT_EQ(*buyer1_usdt_reserved, 0);
    EXPECT_EQ(*buyer1_btc_free, 2);

    const auto buyer2_usdt_free = exchange.free_balance(buyer2_id, Asset{"usdt"});
    const auto buyer2_usdt_reserved = exchange.reserved_balance(buyer2_id, Asset{"usdt"});
    const auto buyer2_btc_free = exchange.free_balance(buyer2_id, Asset{"btc"});
    ASSERT_TRUE(buyer2_usdt_free.has_value());
    ASSERT_TRUE(buyer2_usdt_reserved.has_value());
    ASSERT_TRUE(buyer2_btc_free.has_value());
    EXPECT_EQ(*buyer2_usdt_free, 896);
    EXPECT_EQ(*buyer2_usdt_reserved, 0);
    EXPECT_EQ(*buyer2_btc_free, 1);

    const auto cancel_filled_buy_1 = exchange.cancel_order(buyer1_id, resting_buy_1->order_id);
    ASSERT_FALSE(cancel_filled_buy_1.has_value());
    EXPECT_EQ(cancel_filled_buy_1.error(), CancelOrderError::OrderNotFound);
    const auto cancel_filled_buy_2 = exchange.cancel_order(buyer2_id, resting_buy_2->order_id);
    ASSERT_FALSE(cancel_filled_buy_2.has_value());
    EXPECT_EQ(cancel_filled_buy_2.error(), CancelOrderError::OrderNotFound);
}
