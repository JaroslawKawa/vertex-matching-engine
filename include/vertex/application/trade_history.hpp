#pragma once
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
        std::unordered_map<Market, std::vector<Trade>> trades_map_{};

    public:
        TradeHistory() = default;
        void add(Trade trade);
        std::vector<Trade> market_history(const Market &market) const;
    };

} // namespace vertex::application