#include <gtest/gtest.h>

#include <unordered_set>

#include "vertex/core/types.hpp"

TEST(AssetTest, NormalizesNameToUppercase)
{
    const vertex::core::Asset asset{"bTc"};

    EXPECT_EQ(asset.value(), "BTC");
}

TEST(AssetTest, EqualityIsCaseInsensitiveAfterNormalization)
{
    const vertex::core::Asset lower{"eth"};
    const vertex::core::Asset upper{"ETH"};

    EXPECT_EQ(lower, upper);
}

TEST(AssetTest, HashCanBeUsedInUnorderedSet)
{
    std::unordered_set<vertex::core::Asset> assets;

    assets.insert(vertex::core::Asset{"btc"});
    assets.insert(vertex::core::Asset{"BTC"});
    assets.insert(vertex::core::Asset{"usdt"});

    EXPECT_EQ(assets.size(), 2u);
    EXPECT_TRUE(assets.contains(vertex::core::Asset{"usdt"}));
}

TEST(MarketTest, ExposesBaseAndQuoteAssets)
{
    const vertex::core::Market market{
        vertex::core::Asset{"btc"},
        vertex::core::Asset{"usdt"}};

    EXPECT_EQ(market.base(), vertex::core::Asset{"BTC"});
    EXPECT_EQ(market.quote(), vertex::core::Asset{"USDT"});
}

TEST(MarketTest, EqualityDependsOnBaseAndQuoteOrder)
{
    const vertex::core::Market direct{
        vertex::core::Asset{"btc"},
        vertex::core::Asset{"usdt"}};
    const vertex::core::Market inverse{
        vertex::core::Asset{"usdt"},
        vertex::core::Asset{"btc"}};

    EXPECT_NE(direct, inverse);
}

TEST(MarketTest, HashCanBeUsedInUnorderedSet)
{
    std::unordered_set<vertex::core::Market> markets;

    markets.insert(vertex::core::Market{vertex::core::Asset{"btc"}, vertex::core::Asset{"usdt"}});
    markets.insert(vertex::core::Market{vertex::core::Asset{"BTC"}, vertex::core::Asset{"USDT"}});
    markets.insert(vertex::core::Market{vertex::core::Asset{"eth"}, vertex::core::Asset{"usdt"}});

    EXPECT_EQ(markets.size(), 2u);
    EXPECT_TRUE(markets.contains(vertex::core::Market{vertex::core::Asset{"eth"}, vertex::core::Asset{"usdt"}}));
}
