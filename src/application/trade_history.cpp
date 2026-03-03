#include <cassert>
#include "vertex/application/trade_history.hpp"

namespace vertex::application
{
    std::size_t TradeHistory::shard_index(const Market &market) const
    {
        return std::hash<Market>{}(market) % kShardCount;
    }
    TradeHistory::Shard &TradeHistory::shard_for(const Market &market)
    {
        return shards_[shard_index(market)];
    }

    const TradeHistory::Shard &TradeHistory::shard_for(const Market &market) const
    {
        return shards_[shard_index(market)];
    }

    void TradeHistory::add(Trade trade)
    {
        Shard &shard = shard_for(trade.market());
        {
            std::lock_guard lock(shard.mu_);

            auto x = shard.trades_map_.find(trade.market());
            if (x == shard.trades_map_.end())
            {
                shard.trades_map_.emplace(trade.market(), std::vector{std::move(trade)});
                return;
            }
            x->second.push_back(std::move(trade));
        }
    }

    std::vector<Trade> TradeHistory::market_history(const Market &market) const
    {
        const Shard &shard = shard_for(market);
        {
            std::shared_lock lock(shard.mu_);
            auto result = shard.trades_map_.find(market);
            if (result == shard.trades_map_.end())
                return std::vector<Trade>{};
            return std::vector{result->second};
        }
    }
}