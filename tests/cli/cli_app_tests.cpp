#include <gtest/gtest.h>

#include "app/cli/cli_app.hpp"

namespace
{
    using vertex::cli::AppError;
    using vertex::cli::AppErrorCode;
    using vertex::cli::CancelOrder;
    using vertex::cli::CliApp;
    using vertex::cli::CreateUser;
    using vertex::cli::DepositDone;
    using vertex::cli::GetUser;
    using vertex::cli::LimitOrderPlaced;
    using vertex::cli::MarketRegistered;
    using vertex::cli::OrderCanceled;
    using vertex::cli::PlaceLimitOrder;
    using vertex::cli::RegisterMarket;
    using vertex::cli::UserCreated;
    using vertex::cli::UserRead;
    using vertex::cli::WalletDeposit;

    std::uint64_t require_user_id(const vertex::cli::DispatchResult &result)
    {
        EXPECT_TRUE(std::holds_alternative<UserCreated>(result));
        return std::get<UserCreated>(result).user_id;
    }
}

TEST(CliAppTest, CreateUserThenGetUserReturnsSameIdentity)
{
    CliApp app;

    const auto create_result = app.dispatch(CreateUser{.name = "alice"});
    ASSERT_TRUE(std::holds_alternative<UserCreated>(create_result));
    const auto user_id = std::get<UserCreated>(create_result).user_id;

    const auto get_result = app.dispatch(GetUser{.user_id = user_id});
    ASSERT_TRUE(std::holds_alternative<UserRead>(get_result));
    EXPECT_EQ(std::get<UserRead>(get_result).user_id, user_id);
    EXPECT_EQ(std::get<UserRead>(get_result).name, "alice");
}

TEST(CliAppTest, DuplicateUserNameCreatesDistinctUsers)
{
    CliApp app;

    const auto first_result = app.dispatch(CreateUser{.name = "alice"});
    const auto second_result = app.dispatch(CreateUser{.name = "alice"});

    ASSERT_TRUE(std::holds_alternative<UserCreated>(first_result));
    ASSERT_TRUE(std::holds_alternative<UserCreated>(second_result));

    const auto first_id = std::get<UserCreated>(first_result).user_id;
    const auto second_id = std::get<UserCreated>(second_result).user_id;
    EXPECT_NE(first_id, second_id);
}

TEST(CliAppTest, DepositWithZeroQuantityReturnsInvalidQuantity)
{
    CliApp app;
    const auto user_id = require_user_id(app.dispatch(CreateUser{.name = "alice"}));

    const auto deposit_result = app.dispatch(WalletDeposit{
        .user_id = user_id,
        .asset = "USDT",
        .quantity = 0});

    ASSERT_TRUE(std::holds_alternative<AppError>(deposit_result));
    EXPECT_EQ(std::get<AppError>(deposit_result).code, AppErrorCode::InvalidQuantity);
}

TEST(CliAppTest, RegisterMarketTwiceReturnsAlreadyListed)
{
    CliApp app;

    const auto first = app.dispatch(RegisterMarket{.market = "BTC/USDT"});
    ASSERT_TRUE(std::holds_alternative<MarketRegistered>(first));

    const auto second = app.dispatch(RegisterMarket{.market = "BTC/USDT"});
    ASSERT_TRUE(std::holds_alternative<AppError>(second));
    EXPECT_EQ(std::get<AppError>(second).code, AppErrorCode::MarketAlreadyListed);
}

TEST(CliAppTest, PlaceAndCancelOrderFlowReturnsExpectedResults)
{
    CliApp app;

    const auto buyer_id = require_user_id(app.dispatch(CreateUser{.name = "buyer"}));
    const auto seller_id = require_user_id(app.dispatch(CreateUser{.name = "seller"}));

    ASSERT_TRUE(std::holds_alternative<MarketRegistered>(app.dispatch(RegisterMarket{.market = "BTC/USDT"})));

    const auto buyer_deposit = app.dispatch(WalletDeposit{.user_id = buyer_id, .asset = "USDT", .quantity = 1000});
    ASSERT_TRUE(std::holds_alternative<DepositDone>(buyer_deposit));

    const auto seller_deposit = app.dispatch(WalletDeposit{.user_id = seller_id, .asset = "BTC", .quantity = 5});
    ASSERT_TRUE(std::holds_alternative<DepositDone>(seller_deposit));

    const auto place = app.dispatch(PlaceLimitOrder{
        .user_id = seller_id,
        .market = "BTC/USDT",
        .side = "sell",
        .price = 100,
        .quantity = 2});

    ASSERT_TRUE(std::holds_alternative<LimitOrderPlaced>(place));
    const auto order_id = std::get<LimitOrderPlaced>(place).order_id;

    const auto cancel = app.dispatch(CancelOrder{
        .user_id = seller_id,
        .order_id = order_id});

    ASSERT_TRUE(std::holds_alternative<OrderCanceled>(cancel));
    EXPECT_EQ(std::get<OrderCanceled>(cancel).order_id, order_id);
    EXPECT_EQ(std::get<OrderCanceled>(cancel).side, "Sell");
}
