#include <gtest/gtest.h>

#include "vertex/core/id_generator.hpp"
#include "vertex/core/types.hpp"

TEST(IdGeneratorTest, StartsAtOneAndIncrements)
{
    vertex::core::IdGenerator<vertex::core::UserId> generator;

    const auto first = generator.next();
    const auto second = generator.next();
    const auto third = generator.next();

    EXPECT_EQ(first.get_value(), 1u);
    EXPECT_EQ(second.get_value(), 2u);
    EXPECT_EQ(third.get_value(), 3u);
}

TEST(IdGeneratorTest, GeneratesValidIds)
{
    vertex::core::IdGenerator<vertex::core::OrderId> generator;

    const auto id = generator.next();

    EXPECT_TRUE(id.is_valid());
    EXPECT_GT(id.get_value(), 0u);
}
