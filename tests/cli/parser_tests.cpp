#include <gtest/gtest.h>

#include "app/cli/parser.hpp"

namespace
{
    using vertex::cli::CancelOrder;
    using vertex::cli::CreateUser;
    using vertex::cli::Help;
    using vertex::cli::ParseErrorCode;
    using vertex::cli::ParseStage;
    using vertex::cli::PlaceLimitOrder;
    using vertex::cli::RegisterMarket;
    using vertex::cli::WalletDeposit;
    using vertex::cli::parse_command;
}

TEST(ParserTest, ParsesHelpCommand)
{
    const auto result = parse_command("help");

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<Help>(*result));
}

TEST(ParserTest, ReturnsTooManyArgumentsForHelp)
{
    const auto result = parse_command("help now");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().stage, ParseStage::Parser);
    EXPECT_EQ(result.error().code, ParseErrorCode::TooManyArguments);
}

TEST(ParserTest, ParsesCreateUserWithQuotedName)
{
    const auto result = parse_command("create-user \"Alice Bob\"");

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<CreateUser>(*result));
    EXPECT_EQ(std::get<CreateUser>(*result).name, "Alice Bob");
}

TEST(ParserTest, ParsesDepositCommand)
{
    const auto result = parse_command("deposit 7 USDT 1500");

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<WalletDeposit>(*result));
    const auto &deposit = std::get<WalletDeposit>(*result);
    EXPECT_EQ(deposit.user_id, 7u);
    EXPECT_EQ(deposit.asset, "USDT");
    EXPECT_EQ(deposit.quantity, 1500);
}

TEST(ParserTest, ParsesCancelOrderCommand)
{
    const auto result = parse_command("cancel-order 1 42");

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<CancelOrder>(*result));
    const auto &cancel = std::get<CancelOrder>(*result);
    EXPECT_EQ(cancel.user_id, 1u);
    EXPECT_EQ(cancel.order_id, 42u);
}

TEST(ParserTest, ReturnsTooManyArgumentsForExit)
{
    const auto result = parse_command("exit now");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().stage, ParseStage::Parser);
    EXPECT_EQ(result.error().code, ParseErrorCode::TooManyArguments);
}

TEST(ParserTest, ReturnsUnknownCommandError)
{
    const auto result = parse_command("foo-bar");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().stage, ParseStage::Parser);
    EXPECT_EQ(result.error().code, ParseErrorCode::UnknownCommand);
    EXPECT_EQ(result.error().column, 0u);
}

TEST(ParserTest, ReturnsUnknownCommandErrorWithCorrectColumnAfterLeadingSpaces)
{
    const auto result = parse_command("   foo-bar");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().stage, ParseStage::Parser);
    EXPECT_EQ(result.error().code, ParseErrorCode::UnknownCommand);
    EXPECT_EQ(result.error().column, 3u);
}

TEST(ParserTest, ReturnsInvalidMarketForRegisterMarket)
{
    const auto result = parse_command("register-market BTCUSDT");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().stage, ParseStage::Parser);
    EXPECT_EQ(result.error().code, ParseErrorCode::InvalidMarket);
}

TEST(ParserTest, ReturnsInvalidMarketForSelfMarketWithDifferentCase)
{
    const auto result = parse_command("register-market btc/BTC");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().stage, ParseStage::Parser);
    EXPECT_EQ(result.error().code, ParseErrorCode::InvalidMarket);
}

TEST(ParserTest, ReturnsInvalidSideForPlaceLimit)
{
    const auto result = parse_command("place-limit 1 BTC/USDT hold 100 2");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().stage, ParseStage::Parser);
    EXPECT_EQ(result.error().code, ParseErrorCode::InvalidSide);
}

TEST(ParserTest, ParsesPlaceLimitCommand)
{
    const auto result = parse_command("place-limit 11 BTC/USDT buy 102 3");

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<PlaceLimitOrder>(*result));
    const auto &order = std::get<PlaceLimitOrder>(*result);
    EXPECT_EQ(order.user_id, 11u);
    EXPECT_EQ(order.market, "BTC/USDT");
    EXPECT_EQ(order.side, "buy");
    EXPECT_EQ(order.price, 102);
    EXPECT_EQ(order.quantity, 3);
}

TEST(ParserTest, ParsesRegisterMarketCommand)
{
    const auto result = parse_command("register-market ETH/USDT");

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<RegisterMarket>(*result));
    EXPECT_EQ(std::get<RegisterMarket>(*result).market, "ETH/USDT");
}
