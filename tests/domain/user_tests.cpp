#include <gtest/gtest.h>

#include "vertex/domain/user.hpp"

TEST(UserTest, ExposesIdAndName)
{
    const vertex::core::UserId id{42};
    vertex::domain::User user{id, "Jarek"};

    EXPECT_EQ(user.id(), id);
    EXPECT_EQ(user.name(), "Jarek");
}