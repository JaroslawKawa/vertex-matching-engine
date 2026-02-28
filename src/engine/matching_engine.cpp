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
                    return handle_limit_request(req);
                },
                [this](const MarketBuyByQuoteRequest &req) -> std::vector<Execution>
                {
                    auto order = std::make_unique<MarketOrder>(
                        req.order_id, req.user_id, req.market, Side::Buy, req.quote_budget);
                    return execute_market_order(std::move(order));
                },
                [this](const MarketSellByBaseRequest &req) -> std::vector<Execution>
                {
                    auto order = std::make_unique<MarketOrder>(
                        req.order_id, req.user_id, req.market, Side::Sell, req.base_quantity);
                    return execute_market_order(std::move(order));
                }},
            order_request);
    }

    std::vector<Execution> MatchingEngine::handle_limit_request(const LimitOrderRequest &req)
    {
        assert(has_market(req.market));
        
        auto &book = books_.find(req.market)->second;
        Quantity remaining = req.base_quantity;

        std::vector<Execution> executions = req.side == Side::Buy
                                                ? book.match_limit_buy_against_asks(req.order_id, req.limit_price, remaining)
                                                : book.match_limit_sell_against_bids(req.order_id, req.limit_price, remaining);

        if (remaining > 0)
        {
            RestingOrder ro{
                .order_id = req.order_id,
                .limit_price = req.limit_price,
                .initial_base_quantity = req.base_quantity,
                .remaining_base_quantity = remaining};
            book.insert_resting(req.side, std::move(ro));
        }
        return executions;
    }

}
