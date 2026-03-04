#include "vertex/engine/market_dispatcher.hpp"

namespace vertex::engine
{
    template <class... Ts>
    struct Overloaded : Ts...
    {
        using Ts::operator()...;
    };
    template <class... Ts>
    Overloaded(Ts...) -> Overloaded<Ts...>;
    std::expected<void, EngineAsyncError> MarketDispatcher::register_market(const Market &market)
    {
        bool result = false;
        {
            std::lock_guard lock(workers_mutex_);
            if (stopping_)
            {
                return std::unexpected(EngineAsyncError::WorkerStopped);
            }
            result = workers_.try_emplace(market, std::make_shared<MarketWorker>(market)).second;
        }

        if (!result)
            return std::unexpected(EngineAsyncError::MarketAlreadyRegistered);

        return {};
    }

    bool MarketDispatcher::has_market(const Market &market) const noexcept
    {
        std::shared_lock lock(workers_mutex_);

        return workers_.find(market) != workers_.end();
    }

    std::future<std::expected<std::vector<Execution>, EngineAsyncError>> MarketDispatcher::submit(OrderRequest &&order_request)
    {

        std::shared_ptr<MarketWorker> worker;
        Market market = market_of(order_request);
        {
            std::shared_lock lock(workers_mutex_);
            if (stopping_)
                return make_ready_future_error<std::vector<Execution>>(EngineAsyncError::WorkerStopped);
            auto worker_it = workers_.find(market);

            if (worker_it == workers_.end())
            {
                return make_ready_future_error<std::vector<Execution>>(EngineAsyncError::MarketNotFound);
            }
            worker = worker_it->second;
        }
        return worker->submit(std::move(order_request));
    }

    std::future<std::expected<std::optional<CancelResult>, EngineAsyncError>> MarketDispatcher::cancel(const Market &market, OrderId order_id)
    {

        std::shared_ptr<MarketWorker> worker;
        {
            std::shared_lock lock(workers_mutex_);
            if (stopping_)
                return make_ready_future_error<std::optional<CancelResult>>(EngineAsyncError::WorkerStopped);
            auto worker_it = workers_.find(market);

            if (worker_it == workers_.end())
            {
                return make_ready_future_error<std::optional<CancelResult>>(EngineAsyncError::MarketNotFound);
            }
            worker = worker_it->second;
        }

        return worker->cancel(order_id);
    }

    std::future<std::expected<std::optional<Price>, EngineAsyncError>> MarketDispatcher::best_bid(const Market &market)
    {
        std::shared_ptr<MarketWorker> worker;
        {
            std::shared_lock lock(workers_mutex_);
            if (stopping_)
                return make_ready_future_error<std::optional<Price>>(EngineAsyncError::WorkerStopped);
            auto worker_it = workers_.find(market);

            if (worker_it == workers_.end())
            {
                return make_ready_future_error<std::optional<Price>>(EngineAsyncError::MarketNotFound);
            }
            worker = worker_it->second;
        }

        return worker->best_bid();
    }

    std::future<std::expected<std::optional<Price>, EngineAsyncError>> MarketDispatcher::best_ask(const Market &market)
    {
        std::shared_ptr<MarketWorker> worker;
        {
            std::shared_lock lock(workers_mutex_);
            if (stopping_)
                return make_ready_future_error<std::optional<Price>>(EngineAsyncError::WorkerStopped);
            auto worker_it = workers_.find(market);

            if (worker_it == workers_.end())
            {
                return make_ready_future_error<std::optional<Price>>(EngineAsyncError::MarketNotFound);
            }
            worker = worker_it->second;
        }

        return worker->best_ask();
    }

    MarketDispatcher::~MarketDispatcher()
    {
        stop_all();
    }

    void MarketDispatcher::stop_all()
    {
        std::vector<std::shared_ptr<MarketWorker>> workers;

        {
            std::lock_guard lock(workers_mutex_);
            stopping_ = true;
            workers.reserve(workers_.size());

            for (auto &worker : workers_)
            {
                workers.push_back(worker.second);
            }
        }

        for (auto &worker : workers)
        {
            worker->stop();
        }
    }

    Market MarketDispatcher::market_of(const OrderRequest &order_request)
    {
        return std::visit(
            [](const auto &req) -> const Market &
            { return req.market; },
            order_request);
    }

    template <typename T>
    std::future<std::expected<T, EngineAsyncError>> MarketDispatcher::make_ready_future_error(EngineAsyncError error)
    {
        std::promise<std::expected<T, EngineAsyncError>> p;
        auto f = p.get_future();
        p.set_value(std::unexpected(error));
        return f;
    }

}