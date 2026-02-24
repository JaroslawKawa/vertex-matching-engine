#include <gtest/gtest.h>

#include <unordered_set>

#include "vertex/core/types.hpp"

TEST(StrongIdTest, DefaultConstructedIdIsInvalid)
{
    const vertex::core::UserId id{};

    EXPECT_FALSE(id.is_valid());
    EXPECT_EQ(id.get_value(), 0u);
}

TEST(StrongIdTest, PositiveValueIsValid)
{
    const vertex::core::OrderId id{42};

    EXPECT_TRUE(id.is_valid());
    EXPECT_EQ(id.get_value(), 42u);
}

TEST(StrongIdTest, EqualityAndOrderingWork)
{
    const vertex::core::TradeId low{1};
    const vertex::core::TradeId high{2};

    EXPECT_EQ(low, vertex::core::TradeId{1});
    EXPECT_NE(low, high);
    EXPECT_LT(low, high);
}

TEST(StrongIdTest, HashCanBeUsedInUnorderedSet)
{
    std::unordered_set<vertex::core::UserId> ids;

    ids.insert(vertex::core::UserId{7});
    ids.insert(vertex::core::UserId{8});
    ids.insert(vertex::core::UserId{7});

    EXPECT_EQ(ids.size(), 2u);
    EXPECT_TRUE(ids.contains(vertex::core::UserId{8}));
}
