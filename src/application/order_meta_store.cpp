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

    std::optional<OrderRecord> OrderMetaStore::close_and_extract(OrderId id, OrderStatus status)
    {
        Shard &shard = shard_for(id);

        std::unique_lock lock(shard.mu_);

        auto meta = shard.data.find(id);
        if (meta == shard.data.end())
            return std::nullopt;

        OrderMeta order = std::move(meta->second);

        shard.data.erase(meta);

        lock.unlock();

        std::optional<double> avg_price;
        if (order.executed_base_qty != 0)
        {
            avg_price = std::optional<double>(static_cast<double>(order.executed_quote_qty) / order.executed_base_qty);
        }

        return OrderRecord{.id = id,
                           .user_id = order.owner,
                           .market = order.market,
                           .side = order.side,
                           .type = OrderType::LimitOrder,
                           .status = status,
                           .limit_price = order.price,
                           .requested_base_qty = order.requested_base_qty,
                           .requested_quote_budget = std::nullopt,
                           .executed_base_qty = order.executed_base_qty,
                           .executed_quote_qty = order.executed_quote_qty,
                           .avg_price = avg_price,
                           .fill_count = order.fill_count,
                           .trade_ids = std::move(order.trade_ids)};
    }

    bool OrderMetaStore::append_fill(OrderId id, TradeId trade_id, Quantity qty, Price price)
    {
        Shard &shard = shard_for(id);
        {
            std::lock_guard lock(shard.mu_);

            auto it = shard.data.find(id);

            if (it == shard.data.end())
            {
                return false;
            }

            OrderMeta &meta = it->second;
            meta.executed_base_qty += qty;
            meta.executed_quote_qty += qty * price;
            meta.fill_count += 1;
            meta.trade_ids.push_back(trade_id);

            return true;
        }
    }
}