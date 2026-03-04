#include "vertex/application/order_meta_store.hpp"

namespace vertex::application
{
    std::size_t OrderMetaStore::shard_index(OrderId id) const
    {
        return std::hash<OrderId>{}(id) % kShardCount;
    }

    OrderMetaStore::Shard &OrderMetaStore::shard_for(OrderId id)
    {
        return shards_[shard_index(id)];
    }
    const OrderMetaStore::Shard &OrderMetaStore::shard_for(OrderId id) const
    {
        return shards_[shard_index(id)];
    }

    bool OrderMetaStore::try_insert(OrderId id, OrderMeta meta)
    {

        Shard &shard = shard_for(id);
        {
            std::lock_guard lock(shard.mu_);
            auto [_, result] = shard.data.try_emplace(id, std::move(meta));
            if (!result)
                return false;
        }
        return true;
    }

    std::optional<OrderMeta> OrderMetaStore::find(OrderId id) const
    {
        const Shard &shard = shard_for(id);

        {
            std::lock_guard lock(shard.mu_);
            auto meta = shard.data.find(id);
            if (meta == shard.data.end())
                return std::nullopt;
            return meta->second;
        }
    }
    
    bool OrderMetaStore::erase(OrderId id)
    {
        Shard &shard = shard_for(id);

        {
            std::lock_guard lock(shard.mu_);
            auto meta = shard.data.find(id);
            if (meta == shard.data.end())
                return false;
            shard.data.erase(meta);
            return true;
        }
    }

}