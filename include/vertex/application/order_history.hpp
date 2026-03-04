#pragma once
#include <array>
#include <optional>
#include <mutex>
#include "vertex/core/types.hpp"

namespace vertex::application
{
    using UserId = vertex::core::UserId;
    using OrderId = vertex::core::OrderId;
    using TradeId = vertex::core::TradeId;
    using Market = vertex::core::Market;
    using Side = vertex::core::Side;
    using Price = vertex::core::Price;
    using Quantity = vertex::core::Quantity;

    enum class OrderType
    {
        LimitOrder,
        MarketOrder,
    };
    enum class OrderStatus
    {
        Filled,
        Canceled
    };

    struct OrderRecord
    {
        OrderId id;
        UserId user_id;
        Market market;
        Side side;
        OrderType type;
        OrderStatus status;

        std::optional<Price> limit_price;               // Limit
        std::optional<Quantity> requested_base_qty;     // Limit + MarketSellByBase
        std::optional<Quantity> requested_quote_budget; // MarketBuyByQuote

        Quantity executed_base_qty{0};
        Quantity executed_quote_qty{0};

        std::optional<double> avg_price; // executed_quote_qty / executed_base_qty
        std::size_t fill_count{0};
        std::vector<TradeId> trade_ids;
    };

    class OrderHistory
    {
    private:
        struct ByIdShard
        {
            std::unordered_map<OrderId, OrderRecord> data;
            mutable std::mutex mu_;
        };
        struct ByUserShard
        {
            std::unordered_map<UserId, std::vector<OrderId>> data;
            mutable std::mutex mu_;
        };
        static constexpr std::size_t kShardCount = 64;
        std::array<ByIdShard, 64> by_id_shards_;
        std::array<ByUserShard, 64> by_user_shards_;

        std::size_t shard_by_id_index(OrderId id) const;
        ByIdShard &shard_by_id_for(OrderId id);
        const ByIdShard &shard_by_id_for(OrderId id) const;
        std::size_t shard_by_user_index(UserId id) const;
        ByUserShard &shard_by_user_for(UserId id);
        const ByUserShard &shard_by_user_for(UserId id) const;

    public:
        OrderHistory() = default;
        bool try_insert(OrderRecord order);
        std::optional<OrderRecord> find(OrderId id) const;
        std::optional<std::vector<OrderRecord>> find_by_user(UserId id) const;
    };

} // namespace vertex::application
