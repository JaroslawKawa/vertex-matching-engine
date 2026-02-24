#pragma once
#include <cassert>
#include <compare>
#include <functional>
#include <utility>
#include "vertex/core/asset.hpp"

namespace vertex::core

{
    struct AssetTag;
    using Asset = StrongAsset<AssetTag>;

    class Market
    {
    private:
        Asset base_;
        Asset quote_;

    public:
        Market(Asset base, Asset quote) : base_(std::move(base)), quote_(std::move(quote))
        {
            assert(base_ != quote_);
        }
        const Asset &base() const noexcept
        {
            return base_;
        }
        const Asset &quote() const noexcept
        {
            return quote_;
        }

        auto operator<=>(const Market &other) const = default;
    };

} // namespace vertex::core

namespace std
{
    template <>
    struct hash<vertex::core::Market>
    {
        size_t operator()(const vertex::core::Market &id) const noexcept
        {
            auto h1 = std::hash<vertex::core::Asset>{}(id.base());
            auto h2 = std::hash<vertex::core::Asset>{}(id.quote());
            return h1 ^ (h2 << 1);
        }
    };
}
