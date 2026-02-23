#include <cassert>
#include "vertex/engine/matching_engine.hpp"

namespace vertex::engine
{

    std::vector<Execution> MatchingEngine::add_order(std::unique_ptr<Order> order)
    {
        assert(order != nullptr);

        const Symbol &symbol = order.get()->symbol();

        auto order_book_it = books_.find(symbol);
        assert(order_book_it != books_.end());
        return order_book_it->second.add_order(std::move(order));
    }

    std::optional<CancelResult> MatchingEngine::cancel(const Symbol &symbol, OrderId order_id)
    {
        auto order_book_it = books_.find(symbol);

        assert(order_book_it != books_.end());
        return order_book_it->second.cancel(order_id);
    }

    void MatchingEngine::register_symbol(const Symbol &symbol)
    {
        assert(!has_symbol(symbol));
        books_.emplace(symbol, OrderBook{symbol});
    }

    bool MatchingEngine::has_symbol(const Symbol &symbol) const noexcept
    {
        return (books_.find(symbol) != books_.end());
    }
}