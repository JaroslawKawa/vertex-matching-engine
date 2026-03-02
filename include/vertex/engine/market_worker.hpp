#pragma once
#include <condition_variable>
#include <expected>
#include <future>
#include <thread>
#include <mutex>
#include <vector>
#include <variant>
#include <optional>
#include <queue>
#include <utility>
#include "vertex/engine/order_book.hpp"
#include "vertex/engine/order_request.hpp"
#include "vertex/engine/engine_async_error.hpp"

namespace vertex::engine
{

    using SubmitResult = std::expected<std::vector<Execution>, EngineAsyncError>;
    using CancelResultEx = std::expected<std::optional<CancelResult>, EngineAsyncError>;
    using PriceResult = std::expected<std::optional<Price>, EngineAsyncError>;

    struct SubmitTask
    {
        OrderRequest request;
        std::promise<SubmitResult> done;
    };

    struct CancelTask
    {
        OrderId order_id;
        std::promise<CancelResultEx> done;
    };

    struct BestBidTask
    {
        std::promise<PriceResult> done;
    };

    struct BestAskTask
    {
        std::promise<PriceResult> done;
    };

    using MarketTask = std::variant<SubmitTask, CancelTask, BestBidTask, BestAskTask>;

    class MarketWorker
    {
    public:
        explicit MarketWorker(Market market);
        ~MarketWorker();
        MarketWorker(const MarketWorker &) = delete;
        MarketWorker &operator=(const MarketWorker &) = delete;
        MarketWorker(MarketWorker &&) = delete;
        MarketWorker &operator=(MarketWorker &&) = delete;

        std::future<SubmitResult> submit(OrderRequest request);
        std::future<CancelResultEx> cancel(OrderId order_id);
        std::future<PriceResult> best_bid();
        std::future<PriceResult> best_ask();
        void stop();

    private:
        std::queue<MarketTask> task_queue_{};
        std::thread worker_thread_;
        OrderBook order_book_;
        std::mutex queue_mutex_;
        std::condition_variable queue_cv_;
        bool stopping_{false};

        void run();
        template <typename Task>
        bool try_enqueue(Task &&task);
        std::vector<Execution> handle_submit(const OrderRequest &req);
        std::vector<Execution> handle_limit_request(const LimitOrderRequest &req);
        std::vector<Execution> handle_market_buy_by_quote(const MarketBuyByQuoteRequest &req);
        std::vector<Execution> handle_market_sell_by_base(const MarketSellByBaseRequest &req);
    };

    template <typename Task>
    bool MarketWorker::try_enqueue(Task &&task)
    {
        {
            std::lock_guard lock(queue_mutex_);
            if (stopping_)
                // Important for callers: when this returns false, task was not
                // moved into task_queue_, so caller may still read/finish it.
                return false;

            task_queue_.emplace(std::forward<Task>(task));
        }

        queue_cv_.notify_one();
        return true;
    }
}
