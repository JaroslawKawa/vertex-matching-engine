#include "vertex/domain/user.hpp"

namespace vertex::domain
{

    User::User(UserId id, std::string name) : id_(id), name_(std::move(name)) {}

    UserId User::id() const
    {
        return id_;
    }

    const std::string& User::name() const
    {
        return name_;
    }

} // namespace vertex::domain