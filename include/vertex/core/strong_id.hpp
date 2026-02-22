#pragma once
#include <cstdint>
#include <compare>
#include <functional>


namespace vertex::core
{

    template <typename Tag>
    class StrongId
    {
    private:
        std::uint64_t value_{0};

    public:
        constexpr StrongId() noexcept = default;
        constexpr StrongId(std::uint64_t) noexcept;
        constexpr bool is_valid() const noexcept;
        auto operator<=>(const StrongId &other) const = default;
        constexpr std::uint64_t get_value() const noexcept;
    };
    template <typename Tag>
    constexpr StrongId<Tag>::StrongId(std::uint64_t init) noexcept : value_{init} {};

    template <typename Tag>
    constexpr bool StrongId<Tag>::is_valid() const noexcept
    {
        return value_ != 0;
    }

    template <typename Tag>
    constexpr std::uint64_t StrongId<Tag>::get_value() const noexcept
    {
        return value_;
    }

} // namespace vertex::core

namespace std
{
    template <typename Tag>
    struct hash<vertex::core::StrongId<Tag>>
    {
        size_t operator()(const vertex::core::StrongId<Tag> &id) const noexcept
        {
            return std::hash<std::uint64_t>{}(id.get_value());
        }
    };
}
