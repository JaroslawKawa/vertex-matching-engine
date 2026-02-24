#include <cassert>
#include "vertex/engine/matching_engine.hpp"

namespace vertex::engine
{
    void MatchingEngine::register_market(const Market &market)
    {
        assert(!has_market(market));
        books_.emplace(market, OrderBook{market});
    }

    bool MatchingEngine::has_market(const Market &market) const noexcept
    {
        return (books_.find(market) != books_.end());
    }

    std::vector<Execution> MatchingEngine::add_limit_order(std::unique_ptr<LimitOrder> order)
    {
        assert(order != nullptr);

        const Market &market = order.get()->market();

        auto order_book_it = books_.find(market);
        assert(order_book_it != books_.end());
        return order_book_it->second.add_limit_order(std::move(order));
    }

    std::optional<CancelResult> MatchingEngine::cancel(const Market &market, OrderId order_id)
    {
        auto order_book_it = books_.find(market);

        assert(order_book_it != books_.end());
        return order_book_it->second.cancel(order_id);
    }

    std::optional<Price> MatchingEngine::best_ask(const Market &market) const
    {

        auto order_book_it = books_.find(market);

        assert(order_book_it != books_.end());
        return order_book_it->second.best_ask();
    }

    std::optional<Price> MatchingEngine::best_bid(const Market &market) const
    {

        auto order_book_it = books_.find(market);

        assert(order_book_it != books_.end());
        return order_book_it->second.best_bid();
    }
}