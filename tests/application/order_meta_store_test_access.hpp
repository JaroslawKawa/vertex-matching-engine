#pragma once

#include <mutex>
#include <utility>
#include <vector>

#include "vertex/application/order_meta_store.hpp"

namespace vertex::application
{
    class OrderMetaStoreTestAccess
    {
    public:
        using Snapshot = std::vector<std::pair<OrderId, OrderMeta>>;

        static Snapshot snapshot(const OrderMetaStore &store)
        {
            Snapshot snapshot_entries;

            for (const auto &shard : store.shards_)
            {
                std::lock_guard lock(shard.mu_);
                snapshot_entries.insert(snapshot_entries.end(), shard.data.begin(), shard.data.end());
            }

            return snapshot_entries;
        }
    };
} // namespace vertex::application
