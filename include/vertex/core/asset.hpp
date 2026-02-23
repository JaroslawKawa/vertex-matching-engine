#pragma once
#include <algorithm>
#include <cassert>
#include <cctype>
#include <compare>
#include <functional>
#include <string>

namespace vertex::core
{

    template <typename Tag>
    class StrongAsset
    {
    private:
        std::string name_;

    public:
        explicit StrongAsset(std::string name);
        const std::string &value() const noexcept;
        auto operator<=>(const StrongAsset &other) const = default;
    };

    template <typename Tag>
    StrongAsset<Tag>::StrongAsset(std::string name)
    {
        assert(!name.empty());

        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c)
                       { return std::toupper(c); });

        name_ = std::move(name);
    };

    template <typename Tag>
    const std::string &StrongAsset<Tag>::value() const noexcept
    {
        return name_;
    }
}

namespace std
{
    template <typename Tag>
    struct hash<vertex::core::StrongAsset<Tag>>
    {
        size_t operator()(const vertex::core::StrongAsset<Tag> &id) const noexcept
        {
            return std::hash<std::string>{}(id.value());
        }
    };
}