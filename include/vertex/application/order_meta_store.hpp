#pragma once
#include "vertex/core/types.hpp"
#include "vertex/application/order_history.hpp"
#include <array>
#include <unordered_map>
#include <mutex>
#include <vector>

namespace vertex::application
{
    using UserId = vertex::core::UserId;
    using OrderId = vertex::core::OrderId;
    using Market = vertex::core::Market;
    using Side = vertex::core::Side;
    using Price = vertex::core::Price;
    using TradeId = vertex::core::TradeId;
    using Quantity = vertex::core::Quantity;

    struct OrderMeta
    {
        UserId owner;
        Market market;
        Side side;

        Price price;
        std::optional<Quantity> requested_base_qty; // Limit + MarketSellByBase

        Quantity executed_base_qty{0};
        Quantity executed_quote_qty{0};

        std::size_t fill_count{0};
        std::vector<TradeId> trade_ids{};
    };

    class OrderMetaStoreTestAccess;

    class OrderMetaStore
    {
    private:
        friend class OrderMetaStoreTestAccess;

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
        std::optional<OrderRecord> close_and_extract(OrderId order_id, OrderStatus status);
        bool append_fill(OrderId id, TradeId trade_id, Quantity qty, Price price);
    };
}
