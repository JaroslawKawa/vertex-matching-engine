#include "vertex/engine/market_worker.hpp"

namespace vertex::engine
{

    template <class... Ts>
    struct Overloaded : Ts...
    {
        using Ts::operator()...;
    };
    template <class... Ts>
    Overloaded(Ts...) -> Overloaded<Ts...>;

    MarketWorker::MarketWorker(Market market) : order_book_(OrderBook{market})
    {
        worker_thread_ = std::thread([this]
                                     { run(); });
    }

    MarketWorker::~MarketWorker()
    {
        stop();
        if (worker_thread_.joinable())
            worker_thread_.join();
    }

    std::future<SubmitResult> MarketWorker::submit(OrderRequest request)
    {
        std::promise<SubmitResult> p;
        auto f = p.get_future();

        SubmitTask task = SubmitTask{
            .request = std::move(request),
            .done = std::move(p)};

        if (!try_enqueue(std::move(task)))
        {
            // Safe: try_enqueue returns false only before queue push(std::move(task)),
            // so 'task' still owns a valid promise and we can resolve it here.
            task.done.set_value(std::unexpected(EngineAsyncError::WorkerStopped));
        }

        return f;
    }

    std::future<CancelResultEx> MarketWorker::cancel(OrderId order_id)
    {
        std::promise<CancelResultEx> p;
        auto f = p.get_future();

        CancelTask task = CancelTask{
            .order_id = order_id,
            .done = std::move(p)};

        if (!try_enqueue(std::move(task)))
        {
            // Same rule as submit(): on false path task was not moved into queue.
            task.done.set_value(std::unexpected(EngineAsyncError::WorkerStopped));
        }

        return f;
    }

    std::future<PriceResult> MarketWorker::best_bid()
    {
        std::promise<PriceResult> p;
        auto f = p.get_future();

        BestBidTask task = BestBidTask{.done = std::move(p)};

        if (!try_enqueue(std::move(task)))
        {
            // On enqueue failure we still own the promise in local 'task'.
            task.done.set_value(std::unexpected(EngineAsyncError::WorkerStopped));
        }

        return f;
    }

    std::future<PriceResult> MarketWorker::best_ask()
    {
        std::promise<PriceResult> p;
        auto f = p.get_future();

        BestAskTask task = BestAskTask{.done = std::move(p)};

        if (!try_enqueue(std::move(task)))
        {
            // On enqueue failure we still own the promise in local 'task'.
            task.done.set_value(std::unexpected(EngineAsyncError::WorkerStopped));
        }

        return f;
    }

    void MarketWorker::stop()
    {
        {
            std::lock_guard lock(queue_mutex_);
            stopping_ = true;
        }
        queue_cv_.notify_all();
    }

    void MarketWorker::run()
    {

        while (true)
        {
            std::optional<MarketTask> task;
            {
                std::unique_lock lock(queue_mutex_);
                queue_cv_.wait(lock, [this]
                               { return stopping_ || !task_queue_.empty(); });

                if (stopping_ && task_queue_.empty())
                    return;

                task.emplace(std::move(task_queue_.front()));
                task_queue_.pop();
            }

            std::visit(
                Overloaded{
                    [this](SubmitTask &req) -> void
                    {
                        req.done.set_value(SubmitResult{handle_submit(req.request)});
                    },
                    [this](CancelTask &req) -> void
                    {
                        req.done.set_value(CancelResultEx{order_book_.cancel(req.order_id)});
                    },
                    [this](BestBidTask &req) -> void
                    {
                        req.done.set_value(PriceResult{order_book_.best_bid()});
                    },
                    [this](BestAskTask &req) -> void
                    {
                        req.done.set_value(PriceResult{order_book_.best_ask()});
                    }},
                *task);
        }
    }

    std::vector<Execution> MarketWorker::handle_submit(const OrderRequest &req)
    {
        return std::visit(
            Overloaded{
                [this](const LimitOrderRequest &req) -> std::vector<Execution>
                {
                    return handle_limit_request(req);
                },
                [this](const MarketBuyByQuoteRequest &req) -> std::vector<Execution>
                {
                    return handle_market_buy_by_quote(req);
                },
                [this](const MarketSellByBaseRequest &req) -> std::vector<Execution>
                {
                    return handle_market_sell_by_base(req);
                }},
            req);
    }

    std::vector<Execution> MarketWorker::handle_limit_request(const LimitOrderRequest &req)
    {

        Quantity remaining = req.base_quantity;

        std::vector<Execution> executions = req.side == Side::Buy
                                                ? order_book_.match_limit_buy_against_asks(req.id, req.limit_price, remaining)
                                                : order_book_.match_limit_sell_against_bids(req.id, req.limit_price, remaining);

        if (remaining > 0)
        {
            RestingOrder ro{
                .id = req.id,
                .limit_price = req.limit_price,
                .initial_base_quantity = req.base_quantity,
                .remaining_base_quantity = remaining};
            order_book_.insert_resting(req.side, std::move(ro));
        }

        return executions;
    }

    std::vector<Execution> MarketWorker::handle_market_buy_by_quote(const MarketBuyByQuoteRequest &req)
    {
        return order_book_.match_market_buy_by_quote_against_asks(req.id, req.quote_budget);
    }

    std::vector<Execution> MarketWorker::handle_market_sell_by_base(const MarketSellByBaseRequest &req)
    {
        return order_book_.match_market_sell_by_base_against_bids(req.id, req.base_quantity);
    }

} // namespace vertex::engine
