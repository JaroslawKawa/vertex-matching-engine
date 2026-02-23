#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
#include "vertex/core/types.hpp"
#include "vertex/engine/order_book.hpp"

namespace vertex::engine
{
    using Symbol = vertex::core::Symbol;
    using OrderId = vertex::core::OrderId;

    class MatchingEngine
    {
    private:
        std::unordered_map<Symbol, OrderBook> books_{};

    public:
        MatchingEngine() noexcept = default;
        std::vector<Execution> add_order(std::unique_ptr<Order> order);
        std::optional<CancelResult> cancel(const Symbol &symbol, OrderId order_id);
        void register_symbol(const Symbol &symbol);
        bool has_symbol(const Symbol &symbol) const noexcept;
    };

}