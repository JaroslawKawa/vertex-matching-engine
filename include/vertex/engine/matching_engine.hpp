#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
#include "vertex/core/types.hpp"
#include "vertex/engine/order_book.hpp"
#include "vertex/engine/order_request.hpp"
namespace vertex::engine
{
    using Market = vertex::core::Market;
    using OrderId = vertex::core::OrderId;

    class MatchingEngine
    {
    private:
        std::unordered_map<Market, OrderBook> books_{};


        std::vector<Execution> handle_limit_request(const LimitOrderRequest& req);

    public:
        MatchingEngine() noexcept = default;
        void register_market(const Market &market);
        bool has_market(const Market &market) const noexcept;

        std::vector<Execution> execute_market_order(std::unique_ptr<MarketOrder> order);
        std::optional<CancelResult> cancel(const Market &market, OrderId order_id);
        std::optional<Price> best_ask(const Market &market) const;
        std::optional<Price> best_bid(const Market &market) const;

        std::vector<Execution> submit(OrderRequest&& order_request);
    };

}