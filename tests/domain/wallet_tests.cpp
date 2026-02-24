#include <gtest/gtest.h>

#include "vertex/domain/wallet.hpp"

namespace
{
    using vertex::core::Asset;
    using vertex::domain::Wallet;
    using vertex::domain::WalletError;

    Asset btc()
    {
        return Asset{"btc"};
    }

    Asset usdt()
    {
        return Asset{"usdt"};
    }
}

TEST(WalletTest, DepositCreatesAssetAndIncreasesFreeBalance)
{
    Wallet wallet;

    auto result = wallet.deposit(btc(), 10);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(wallet.free_balance(btc()), 10);
    EXPECT_EQ(wallet.reserved_balance(btc()), 0);
}

TEST(WalletTest, DepositReturnsInvalidAmountForNonPositive)
{
    Wallet wallet;

    auto zero = wallet.deposit(btc(), 0);
    auto negative = wallet.deposit(btc(), -5);

    ASSERT_FALSE(zero.has_value());
    ASSERT_FALSE(negative.has_value());
    EXPECT_EQ(zero.error(), WalletError::InvalidAmount);
    EXPECT_EQ(negative.error(), WalletError::InvalidAmount);
}

TEST(WalletTest, WithdrawUpdatesFreeBalance)
{
    Wallet wallet;
    ASSERT_TRUE(wallet.deposit(usdt(), 100).has_value());

    auto withdraw_result = wallet.withdraw(usdt(), 40);

    ASSERT_TRUE(withdraw_result.has_value());
    EXPECT_EQ(wallet.free_balance(usdt()), 60);
}

TEST(WalletTest, WithdrawReturnsInsufficientFundsWhenAssetMissing)
{
    Wallet wallet;

    auto result = wallet.withdraw(usdt(), 1);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), WalletError::InsufficientFunds);
}

TEST(WalletTest, ReserveMovesFundsFromFreeToReserved)
{
    Wallet wallet;
    ASSERT_TRUE(wallet.deposit(btc(), 7).has_value());

    auto result = wallet.reserve(btc(), 3);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(wallet.free_balance(btc()), 4);
    EXPECT_EQ(wallet.reserved_balance(btc()), 3);
}

TEST(WalletTest, ReleaseMovesFundsFromReservedToFree)
{
    Wallet wallet;
    ASSERT_TRUE(wallet.deposit(btc(), 7).has_value());
    ASSERT_TRUE(wallet.reserve(btc(), 5).has_value());

    auto result = wallet.release(btc(), 2);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(wallet.free_balance(btc()), 4);
    EXPECT_EQ(wallet.reserved_balance(btc()), 3);
}

TEST(WalletTest, ConsumeReservedDecreasesReservedOnly)
{
    Wallet wallet;
    ASSERT_TRUE(wallet.deposit(usdt(), 100).has_value());
    ASSERT_TRUE(wallet.reserve(usdt(), 70).has_value());

    auto result = wallet.consume_reserved(usdt(), 30);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(wallet.free_balance(usdt()), 30);
    EXPECT_EQ(wallet.reserved_balance(usdt()), 40);
}

TEST(WalletTest, MissingAssetBalancesReturnZero)
{
    Wallet wallet;

    EXPECT_EQ(wallet.free_balance(btc()), 0);
    EXPECT_EQ(wallet.reserved_balance(btc()), 0);
}