#include <cassert>
#include <variant>

#include "vertex/engine/matching_engine.hpp"

namespace vertex::engine
{
    template <class... Ts>
    struct Overloaded : Ts...
    {
        using Ts::operator()...;
    };
    template <class... Ts>
    Overloaded(Ts...) -> Overloaded<Ts...>;

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

    std::vector<Execution> MatchingEngine::execute_market_order(std::unique_ptr<MarketOrder> order)
    {
        assert(order != nullptr);

        const Market &market = order->market();
        auto order_book_it = books_.find(market);
        assert(order_book_it != books_.end());
        return order_book_it->second.execute_market_order(std::move(order));
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

    std::vector<Execution> MatchingEngine::submit(const OrderRequest &order_request)
    {
        return std::visit(
            Overloaded{
                [this](const LimitOrderRequest &req) -> std::vector<Execution>
                {
                    auto order = std::make_unique<LimitOrder>(
                        req.request_id, req.user_id, req.market, req.side, req.base_quantity, req.limit_price);
                    return add_limit_order(std::move(order));
                },
                [this](const MarketBuyByQuoteRequest &req) -> std::vector<Execution>
                {
                    auto order = std::make_unique<MarketOrder>(
                        req.request_id, req.user_id, req.market, Side::Buy, req.quote_budget);
                    return execute_market_order(std::move(order));
                },
                [this](const MarketSellByBaseRequest &req) -> std::vector<Execution>
                {
                    auto order = std::make_unique<MarketOrder>(
                        req.request_id, req.user_id, req.market, Side::Sell, req.base_quantity);
                    return execute_market_order(std::move(order));
                }},
            order_request);
    }

}
