#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
#include "vertex/core/types.hpp"
#include "vertex/engine/order_book.hpp"

namespace vertex::engine
{
    using Market = vertex::core::Market;
    using OrderId = vertex::core::OrderId;

    class MatchingEngine
    {
    private:
        std::unordered_map<Market, OrderBook> books_{};

    public:
        MatchingEngine() noexcept = default;
        void register_market(const Market &market);
        bool has_market(const Market &market) const noexcept;

        std::vector<Execution> add_limit_order(std::unique_ptr<LimitOrder> order);
        std::optional<CancelResult> cancel(const Market &market, OrderId order_id);
        std::optional<Price> best_ask(const Market &market) const;
        std::optional<Price> best_bid(const Market &market) const;
    };

}