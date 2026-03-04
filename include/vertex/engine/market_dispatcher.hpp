#pragma once
#include <expected>
#include <future>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <vertex/engine/market_worker.hpp>
#include <vertex/engine/engine_async_error.hpp>

namespace vertex::engine
{
    class MarketDispatcher
    {
    private:
        std::unordered_map<Market, std::shared_ptr<MarketWorker>> workers_{};
        mutable std::shared_mutex workers_mutex_;
        bool stopping_{false};
        
        Market market_of(const OrderRequest &);

        template <typename T>
        std::future<std::expected<T, EngineAsyncError>> make_ready_future_error(EngineAsyncError error);

    public:
        MarketDispatcher() = default;
        ~MarketDispatcher();

        std::expected<void, EngineAsyncError> register_market(const Market &market);
        bool has_market(const Market &market) const noexcept;

        std::future<std::expected<std::vector<Execution>, EngineAsyncError>> submit(OrderRequest &&order_request);
        std::future<std::expected<std::optional<CancelResult>, EngineAsyncError>> cancel(const Market &market, OrderId order_id);
        std::future<std::expected<std::optional<Price>, EngineAsyncError>> best_bid(const Market &market);
        std::future<std::expected<std::optional<Price>, EngineAsyncError>> best_ask(const Market &market);
        void stop_all();
    };

}