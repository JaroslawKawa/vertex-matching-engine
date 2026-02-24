#include <cassert>
#include "vertex/application/trade_history.hpp"

namespace vertex::application
{

    void TradeHistory::add(Trade trade)
    {
        trades_map_[trade.market()].push_back(std::move(trade));
    }

    std::vector<Trade> TradeHistory::market_history(const Market &market) const
    {
        auto result = trades_map_.find(market);
        if (result == trades_map_.end())
            return std::vector<Trade>{};

        if (result->second.empty())
            return std::vector<Trade>{};

        return result->second;
    }
}