#pragma once
#include "vertex/core/types.hpp"
#include <array>
#include <unordered_map>
#include <mutex>

namespace vertex::application
{
    using UserId = vertex::core::UserId;
    using OrderId = vertex::core::OrderId;
    using Market = vertex::core::Market;
    using Side = vertex::core::Side;
    using Price = vertex::core::Price;

    struct OrderMeta
    {
        UserId owner;
        Market market;
        Side side;
        Price price;
    };

    class OrderMetaStore
    {
    private:
        struct Shard
        {
            std::unordered_map<OrderId, OrderMeta> data;
            mutable std::mutex mu_;
        };
        static constexpr std::size_t kShardCount = 64;
        std::array<Shard, kShardCount> shards_;

        std::size_t shard_index(OrderId id) const;
        Shard &shard_for(OrderId id);
        const Shard &shard_for(OrderId id) const;

    public:
        OrderMetaStore() = default;
        bool try_insert(OrderId id, OrderMeta meta);
        std::optional<OrderMeta> find(OrderId) const;
        bool erase(OrderId id);
    };
}