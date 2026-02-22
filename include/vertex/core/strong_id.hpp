#pragma once
#include <cstdint>
#include <compare>

namespace vertex::core
{
    template <typename Tag>
    class StrongId
    {
    private:
        std::uint64_t value_ = {0};

    public:
        StrongId() = default;
        explicit StrongId(std::uint64_t);
        bool is_valid() const;
        auto operator<=>(const StrongId &other) const = default;
        std::uint64_t get_value() const;
    };
    template <typename Tag>
    StrongId<Tag>::StrongId(std::uint64_t init) : value_{init} {};

    template <typename Tag>
    bool StrongId<Tag>::is_valid() const
    {
        return value_ != 0;
    }

    template <typename Tag>
    std::uint64_t StrongId<Tag>::get_value() const
    {
        return value_;
    }

    //TO DO HashFunction

}// namespace vertex::core
