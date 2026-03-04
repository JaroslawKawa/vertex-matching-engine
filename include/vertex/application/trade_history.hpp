#pragma once
#include <array>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "vertex/core/types.hpp"
#include "vertex/core/market.hpp"
#include "vertex/domain/trade.hpp"

namespace vertex::application
{
    using Market = vertex::core::Market;
    using Trade = vertex::domain::Trade;
    class TradeHistory
    {
    private:
        struct Shard
        {
            std::unordered_map<Market, std::vector<Trade>> trades_map_{};
            mutable std::mutex mu_;
        };
        static constexpr std::size_t kShardCount = 64;
        std::array<Shard, kShardCount> shards_;

        std::size_t shard_index(const Market &market) const;
        Shard &shard_for(const Market &market);
        const Shard &shard_for(const Market &market) const;

    public:
        TradeHistory() = default;
        void add(Trade trade);
        std::vector<Trade> market_history(const Market &market) const;
    };

} // namespace vertex::application
