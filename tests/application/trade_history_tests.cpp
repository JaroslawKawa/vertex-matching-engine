#include <gtest/gtest.h>

#include "vertex/application/trade_history.hpp"

namespace
{
    using vertex::application::TradeHistory;
    using vertex::core::Asset;
    using vertex::core::Market;
    using vertex::core::OrderId;
    using vertex::core::TradeId;
    using vertex::core::UserId;
    using vertex::domain::Trade;

    Market btc_usdt()
    {
        return Market{Asset{"btc"}, Asset{"usdt"}};
    }

    Market eth_usdt()
    {
        return Market{Asset{"eth"}, Asset{"usdt"}};
    }
}

TEST(TradeHistoryTest, EmptyHistoryForUnknownMarket)
{
    TradeHistory history;

    const auto trades = history.market_history(btc_usdt());

    EXPECT_TRUE(trades.empty());
}

TEST(TradeHistoryTest, StoresTradesPerMarket)
{
    TradeHistory history;

    history.add(Trade{TradeId{1}, UserId{10}, UserId{20}, OrderId{100}, OrderId{200}, btc_usdt(), 2, 50000});
    history.add(Trade{TradeId{2}, UserId{11}, UserId{21}, OrderId{101}, OrderId{201}, btc_usdt(), 1, 51000});
    history.add(Trade{TradeId{3}, UserId{12}, UserId{22}, OrderId{102}, OrderId{202}, eth_usdt(), 5, 3000});

    const auto btc_trades = history.market_history(btc_usdt());
    const auto eth_trades = history.market_history(eth_usdt());

    ASSERT_EQ(btc_trades.size(), 2u);
    ASSERT_EQ(eth_trades.size(), 1u);
    EXPECT_EQ(btc_trades[0].id(), TradeId{1});
    EXPECT_EQ(btc_trades[1].id(), TradeId{2});
    EXPECT_EQ(eth_trades[0].id(), TradeId{3});
}
