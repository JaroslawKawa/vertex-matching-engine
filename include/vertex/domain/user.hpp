#pragma once

#include <string>
#include "vertex/core/types.hpp"

namespace vertex::domain

{
    using UserId = vertex::core::UserId;

    class User
    {
    private:
        UserId id_;
        std::string name_;

    public:
        User(UserId id, std::string name);
        auto operator<=>(const User& other) const = default;
        UserId id() const;
        const std::string& name() const;
    };

} // namespace vertex::domain