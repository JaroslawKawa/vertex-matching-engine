#include <unordered_map>
#include <vector>

#include "vertex/application/order_history.hpp"

namespace vertex::application
{
    std::size_t OrderHistory::shard_by_id_index(OrderId id) const
    {
        return std::hash<OrderId>{}(id) % kShardCount;
    }

    OrderHistory::ByIdShard &OrderHistory::shard_by_id_for(OrderId id)
    {
        return by_id_shards_[shard_by_id_index(id)];
    }
    const OrderHistory::ByIdShard &OrderHistory::shard_by_id_for(OrderId id) const
    {
        return by_id_shards_[shard_by_id_index(id)];
    }

    std::size_t OrderHistory::shard_by_user_index(UserId id) const
    {
        return std::hash<UserId>{}(id) % kShardCount;
    }

    OrderHistory::ByUserShard &OrderHistory::shard_by_user_for(UserId id)
    {
        return by_user_shards_[shard_by_user_index(id)];
    }
    const OrderHistory::ByUserShard &OrderHistory::shard_by_user_for(UserId id) const
    {
        return by_user_shards_[shard_by_user_index(id)];
    }

    bool OrderHistory::try_insert(OrderRecord order)
    {

        ByIdShard &id_shard = shard_by_id_for(order.id);
        ByUserShard &user_shard = shard_by_user_for(order.user_id);

        OrderId id = order.id;
        UserId user_id = order.user_id;
        std::scoped_lock lock(id_shard.mu_, user_shard.mu_);

        auto [it_id, inserted_id] = id_shard.data.try_emplace(order.id, std::move(order));
        if (!inserted_id)
            return false;

        auto [it_user, inserted_user] = user_shard.data.try_emplace(user_id, std::vector<OrderId>{});
        it_user->second.push_back(id);

        return true;
    }

    std::optional<OrderRecord> OrderHistory::find(OrderId id) const
    {
        const ByIdShard &id_shard = shard_by_id_for(id);

        std::lock_guard lock(id_shard.mu_);
        auto it_order_record = id_shard.data.find(id);

        if (it_order_record == id_shard.data.end())
            return std::nullopt;

        return it_order_record->second;
    }
    std::optional<std::vector<OrderRecord>> OrderHistory::find_by_user(UserId id) const
    {
        const ByUserShard &user_shard = shard_by_user_for(id);

        std::vector<OrderId> order_ids;

        {
            std::lock_guard lock(user_shard.mu_);
            auto it_order_record = user_shard.data.find(id);

            if (it_order_record == user_shard.data.end())
                return std::nullopt;
            order_ids = it_order_record->second;
        }

        std::vector<OrderRecord> result;

        for (const auto &order_id : order_ids)
        {
            auto record = find(order_id);
            if (!record)
                continue;
            result.push_back(record.value());
        }

        return result;
    }
}