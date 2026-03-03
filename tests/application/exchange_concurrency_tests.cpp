#include <atomic>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>

#include "tests/application/exchange_test_access.hpp"
#include "vertex/application/exchange.hpp"
#include "vertex/application/trade_history.hpp"

namespace
{
    using vertex::application::Exchange;
    using vertex::application::CancelOrderError;
    using vertex::application::ExchangeTestAccess;
    using vertex::application::TradeHistory;
    using vertex::core::Asset;
    using vertex::core::Market;
    using vertex::core::OrderId;
    using vertex::core::Side;
    using vertex::core::TradeId;
    using vertex::core::UserId;
    using vertex::domain::Trade;

    class TimeoutAbortGuard
    {
    public:
        explicit TimeoutAbortGuard(std::chrono::milliseconds timeout)
            : watchdog_([this, timeout]() {
                  std::unique_lock lock(mu_);
                  if (!cv_.wait_for(lock, timeout, [this]() { return done_; }))
                  {
                      std::abort();
                  }
              })
        {
        }

        ~TimeoutAbortGuard()
        {
            {
                std::lock_guard lock(mu_);
                done_ = true;
            }
            cv_.notify_one();
            watchdog_.join();
        }

    private:
        std::mutex mu_;
        std::condition_variable cv_;
        bool done_{false};
        std::thread watchdog_;
    };

    Market btc_usdt()
    {
        return Market{Asset{"btc"}, Asset{"usdt"}};
    }

    void expect_no_orphan_orders(const Exchange &exchange, const std::vector<Market> &known_markets)
    {
        const auto snapshot = ExchangeTestAccess::order_meta_snapshot(exchange);

        for (const auto &[order_id, meta] : snapshot)
        {
            (void)order_id;
            EXPECT_TRUE(exchange.user_exists(meta.owner));
            const bool market_known = std::find(known_markets.begin(), known_markets.end(), meta.market) != known_markets.end();
            EXPECT_TRUE(market_known);
        }
    }
}

TEST(ExchangeConcurrencyTest, ConcurrentDepositWithdrawOnSameUserKeepsBalancesConsistent)
{
    Exchange exchange;
    const auto user_result = exchange.create_user("shared-user");
    ASSERT_TRUE(user_result.has_value());
    const UserId user_id = *user_result;
    const Asset usdt{"usdt"};

    constexpr int kThreads = 8;
    constexpr int kIterationsPerThread = 2000;
    constexpr int kInitialBalance = 10000;

    ASSERT_TRUE(exchange.deposit(user_id, usdt, kInitialBalance).has_value());

    std::atomic<bool> start{false};
    std::atomic<bool> ok{true};
    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([&]() {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            for (int i = 0; i < kIterationsPerThread; ++i)
            {
                if (!exchange.deposit(user_id, usdt, 1).has_value())
                {
                    ok.store(false, std::memory_order_release);
                    break;
                }
                if (!exchange.withdraw(user_id, usdt, 1).has_value())
                {
                    ok.store(false, std::memory_order_release);
                    break;
                }
            }
        });
    }

    start.store(true, std::memory_order_release);
    for (auto &thread : threads)
    {
        thread.join();
    }

    EXPECT_TRUE(ok.load(std::memory_order_acquire));

    const auto free = exchange.free_balance(user_id, usdt);
    const auto reserved = exchange.reserved_balance(user_id, usdt);
    ASSERT_TRUE(free.has_value());
    ASSERT_TRUE(reserved.has_value());
    EXPECT_EQ(*free, kInitialBalance);
    EXPECT_EQ(*reserved, 0);
}

TEST(ExchangeConcurrencyTest, ConcurrentOperationsOnDifferentUsersStayIsolated)
{
    Exchange exchange;
    const Asset usdt{"usdt"};

    constexpr int kUsers = 8;
    constexpr int kIterationsPerThread = 1500;
    constexpr int kInitialBalance = 5000;

    std::vector<UserId> users;
    users.reserve(kUsers);
    for (int i = 0; i < kUsers; ++i)
    {
        const auto user_result = exchange.create_user("user-" + std::to_string(i));
        ASSERT_TRUE(user_result.has_value());
        users.push_back(*user_result);
        ASSERT_TRUE(exchange.deposit(users.back(), usdt, kInitialBalance).has_value());
    }

    std::atomic<bool> start{false};
    std::atomic<bool> ok{true};
    std::vector<std::thread> threads;
    threads.reserve(kUsers);

    for (int i = 0; i < kUsers; ++i)
    {
        const UserId user_id = users[i];
        threads.emplace_back([&exchange, &start, &ok, user_id, usdt]() {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            for (int iter = 0; iter < kIterationsPerThread; ++iter)
            {
                if (!exchange.deposit(user_id, usdt, 2).has_value())
                {
                    ok.store(false, std::memory_order_release);
                    break;
                }
                if (!exchange.reserve(user_id, usdt, 1).has_value())
                {
                    ok.store(false, std::memory_order_release);
                    break;
                }
                if (!exchange.release(user_id, usdt, 1).has_value())
                {
                    ok.store(false, std::memory_order_release);
                    break;
                }
                if (!exchange.withdraw(user_id, usdt, 2).has_value())
                {
                    ok.store(false, std::memory_order_release);
                    break;
                }
            }
        });
    }

    start.store(true, std::memory_order_release);
    for (auto &thread : threads)
    {
        thread.join();
    }

    EXPECT_TRUE(ok.load(std::memory_order_acquire));
    for (const UserId user_id : users)
    {
        const auto free = exchange.free_balance(user_id, usdt);
        const auto reserved = exchange.reserved_balance(user_id, usdt);
        ASSERT_TRUE(free.has_value());
        ASSERT_TRUE(reserved.has_value());
        EXPECT_EQ(*free, kInitialBalance);
        EXPECT_EQ(*reserved, 0);
    }
}

TEST(TradeHistoryConcurrencyTest, ConcurrentAddAndConcurrentReadAreStable)
{
    TradeHistory history;
    const Market market = btc_usdt();

    constexpr int kWriterThreads = 4;
    constexpr int kTradesPerWriter = 1000;
    constexpr int kReaderThreads = 8;
    constexpr int kReadsPerReader = 300;

    std::atomic<bool> start{false};
    std::atomic<bool> ok{true};
    std::vector<std::thread> writers;
    writers.reserve(kWriterThreads);

    for (int w = 0; w < kWriterThreads; ++w)
    {
        writers.emplace_back([&, w]() {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            for (int i = 0; i < kTradesPerWriter; ++i)
            {
                const std::uint64_t id = static_cast<std::uint64_t>(w * kTradesPerWriter + i + 1);
                history.add(Trade{
                    TradeId{id},
                    UserId{id + 10000},
                    UserId{id + 20000},
                    OrderId{id + 30000},
                    OrderId{id + 40000},
                    market,
                    1,
                    100});
            }
        });
    }

    start.store(true, std::memory_order_release);
    for (auto &writer : writers)
    {
        writer.join();
    }

    EXPECT_TRUE(ok.load(std::memory_order_acquire));

    const auto final_history = history.market_history(market);
    const auto expected_size = static_cast<std::size_t>(kWriterThreads * kTradesPerWriter);
    ASSERT_EQ(final_history.size(), expected_size);

    std::unordered_set<std::uint64_t> ids;
    ids.reserve(final_history.size());
    for (const auto &trade : final_history)
    {
        ids.insert(trade.id().get_value());
    }
    EXPECT_EQ(ids.size(), final_history.size());

    std::vector<std::thread> readers;
    readers.reserve(kReaderThreads);

    for (int r = 0; r < kReaderThreads; ++r)
    {
        readers.emplace_back([&]() {
            for (int i = 0; i < kReadsPerReader; ++i)
            {
                const auto snapshot = history.market_history(market);
                if (snapshot.size() != expected_size)
                {
                    ok.store(false, std::memory_order_release);
                    return;
                }
                for (const auto &trade : snapshot)
                {
                    if (trade.market() != market)
                    {
                        ok.store(false, std::memory_order_release);
                        return;
                    }
                }
            }
        });
    }

    for (auto &reader : readers)
    {
        reader.join();
    }

    EXPECT_TRUE(ok.load(std::memory_order_acquire));
}

TEST(ExchangeConcurrencyTest, CancelVsFillDoesNotDeadlock)
{
    Exchange exchange;
    const Market market = btc_usdt();
    const Asset btc{"btc"};
    const Asset usdt{"usdt"};

    ASSERT_TRUE(exchange.register_market(market).has_value());

    const auto seller_result = exchange.create_user("seller-deadlock");
    const auto buyer_result = exchange.create_user("buyer-deadlock");
    ASSERT_TRUE(seller_result.has_value());
    ASSERT_TRUE(buyer_result.has_value());
    const UserId seller = *seller_result;
    const UserId buyer = *buyer_result;

    constexpr int kInitialOrders = 300;
    constexpr int kOpsPerThread = 900;
    ASSERT_TRUE(exchange.deposit(seller, btc, kInitialOrders + 1000).has_value());
    ASSERT_TRUE(exchange.deposit(buyer, usdt, kOpsPerThread + 1000).has_value());

    std::vector<OrderId> order_ids;
    order_ids.reserve(kInitialOrders);
    for (int i = 0; i < kInitialOrders; ++i)
    {
        const auto place = exchange.place_limit_order(seller, market, Side::Sell, 1, 1);
        ASSERT_TRUE(place.has_value());
        order_ids.push_back(place->order_id);
    }

    std::atomic<bool> start{false};
    std::atomic<bool> ok{true};
    std::atomic<int> cancel_attempts{0};
    std::atomic<int> fill_attempts{0};

    TimeoutAbortGuard guard(std::chrono::milliseconds(8000));

    std::thread cancel_thread([&]() {
        while (!start.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }

        for (int i = 0; i < kOpsPerThread; ++i)
        {
            const OrderId id = order_ids[static_cast<std::size_t>(i % kInitialOrders)];
            const auto cancel_result = exchange.cancel_order(seller, id);
            if (!cancel_result.has_value() && cancel_result.error() != CancelOrderError::OrderNotFound)
            {
                ok.store(false, std::memory_order_release);
                break;
            }
            cancel_attempts.fetch_add(1, std::memory_order_acq_rel);
        }
    });

    std::thread fill_thread([&]() {
        while (!start.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }

        for (int i = 0; i < kOpsPerThread; ++i)
        {
            const auto market_buy = exchange.execute_market_order(buyer, market, Side::Buy, 1);
            if (!market_buy.has_value())
            {
                ok.store(false, std::memory_order_release);
                break;
            }
            fill_attempts.fetch_add(1, std::memory_order_acq_rel);
        }
    });

    start.store(true, std::memory_order_release);
    cancel_thread.join();
    fill_thread.join();

    EXPECT_TRUE(ok.load(std::memory_order_acquire));
    EXPECT_EQ(cancel_attempts.load(std::memory_order_acquire), kOpsPerThread);
    EXPECT_EQ(fill_attempts.load(std::memory_order_acquire), kOpsPerThread);
}

TEST(ExchangeConcurrencyTest, PlaceLimitParallelStressDoesNotProduceNegativeBalances)
{
    Exchange exchange;
    const Market market = btc_usdt();
    const Asset btc{"btc"};
    const Asset usdt{"usdt"};

    ASSERT_TRUE(exchange.register_market(market).has_value());

    constexpr int kBuyUsers = 4;
    constexpr int kSellUsers = 4;
    constexpr int kIterations = 1200;
    constexpr int kPrice = 100;

    std::vector<UserId> buy_users;
    std::vector<UserId> sell_users;
    buy_users.reserve(kBuyUsers);
    sell_users.reserve(kSellUsers);

    for (int i = 0; i < kBuyUsers; ++i)
    {
        const auto user = exchange.create_user("stress-buy-" + std::to_string(i));
        ASSERT_TRUE(user.has_value());
        buy_users.push_back(*user);
        ASSERT_TRUE(exchange.deposit(*user, usdt, 250000).has_value());
    }

    for (int i = 0; i < kSellUsers; ++i)
    {
        const auto user = exchange.create_user("stress-sell-" + std::to_string(i));
        ASSERT_TRUE(user.has_value());
        sell_users.push_back(*user);
        ASSERT_TRUE(exchange.deposit(*user, btc, 3000).has_value());
    }

    std::atomic<bool> start{false};
    std::atomic<bool> ok{true};
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;
    threads.reserve(kBuyUsers + kSellUsers);

    for (const UserId user_id : buy_users)
    {
        threads.emplace_back([&exchange, &market, &start, &ok, &success_count, user_id]() {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
            for (int i = 0; i < kIterations; ++i)
            {
                const auto result = exchange.place_limit_order(user_id, market, Side::Buy, kPrice, 1);
                if (!result.has_value())
                {
                    ok.store(false, std::memory_order_release);
                    break;
                }
                success_count.fetch_add(1, std::memory_order_acq_rel);
            }
        });
    }

    for (const UserId user_id : sell_users)
    {
        threads.emplace_back([&exchange, &market, &start, &ok, &success_count, user_id]() {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
            for (int i = 0; i < kIterations; ++i)
            {
                const auto result = exchange.place_limit_order(user_id, market, Side::Sell, kPrice, 1);
                if (!result.has_value())
                {
                    ok.store(false, std::memory_order_release);
                    break;
                }
                success_count.fetch_add(1, std::memory_order_acq_rel);
            }
        });
    }

    start.store(true, std::memory_order_release);
    for (auto &thread : threads)
    {
        thread.join();
    }

    EXPECT_TRUE(ok.load(std::memory_order_acquire));
    EXPECT_GT(success_count.load(std::memory_order_acquire), 0);

    for (const UserId user_id : buy_users)
    {
        const auto free_quote = exchange.free_balance(user_id, usdt);
        const auto reserved_quote = exchange.reserved_balance(user_id, usdt);
        const auto free_base = exchange.free_balance(user_id, btc);
        const auto reserved_base = exchange.reserved_balance(user_id, btc);
        ASSERT_TRUE(free_quote.has_value());
        ASSERT_TRUE(reserved_quote.has_value());
        ASSERT_TRUE(free_base.has_value());
        ASSERT_TRUE(reserved_base.has_value());
        EXPECT_GE(*free_quote, 0);
        EXPECT_GE(*reserved_quote, 0);
        EXPECT_GE(*free_base, 0);
        EXPECT_GE(*reserved_base, 0);
    }

    for (const UserId user_id : sell_users)
    {
        const auto free_quote = exchange.free_balance(user_id, usdt);
        const auto reserved_quote = exchange.reserved_balance(user_id, usdt);
        const auto free_base = exchange.free_balance(user_id, btc);
        const auto reserved_base = exchange.reserved_balance(user_id, btc);
        ASSERT_TRUE(free_quote.has_value());
        ASSERT_TRUE(reserved_quote.has_value());
        ASSERT_TRUE(free_base.has_value());
        ASSERT_TRUE(reserved_base.has_value());
        EXPECT_GE(*free_quote, 0);
        EXPECT_GE(*reserved_quote, 0);
        EXPECT_GE(*free_base, 0);
        EXPECT_GE(*reserved_base, 0);
    }

    expect_no_orphan_orders(exchange, {market});
}

TEST(ExchangeConcurrencyTest, MarketOrderAndCancelMixedPreservesReserveInvariants)
{
    Exchange exchange;
    const Market market = btc_usdt();
    const Asset btc{"btc"};
    const Asset usdt{"usdt"};

    ASSERT_TRUE(exchange.register_market(market).has_value());

    const auto seller_result = exchange.create_user("seller-mixed");
    const auto buyer_result = exchange.create_user("buyer-mixed");
    ASSERT_TRUE(seller_result.has_value());
    ASSERT_TRUE(buyer_result.has_value());
    const UserId seller = *seller_result;
    const UserId buyer = *buyer_result;

    constexpr int kInitialOrders = 2000;
    constexpr int kMarketOps = 2000;
    const std::int64_t initial_seller_btc = kInitialOrders;
    const std::int64_t initial_buyer_usdt = kMarketOps;

    ASSERT_TRUE(exchange.deposit(seller, btc, initial_seller_btc).has_value());
    ASSERT_TRUE(exchange.deposit(buyer, usdt, initial_buyer_usdt).has_value());

    std::vector<OrderId> order_ids;
    order_ids.reserve(kInitialOrders);
    for (int i = 0; i < kInitialOrders; ++i)
    {
        const auto place = exchange.place_limit_order(seller, market, Side::Sell, 1, 1);
        ASSERT_TRUE(place.has_value());
        order_ids.push_back(place->order_id);
    }

    std::atomic<bool> start{false};
    std::atomic<bool> ok{true};

    std::thread market_thread([&]() {
        while (!start.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
        for (int i = 0; i < kMarketOps; ++i)
        {
            const auto result = exchange.execute_market_order(buyer, market, Side::Buy, 1);
            if (!result.has_value())
            {
                ok.store(false, std::memory_order_release);
                break;
            }
        }
    });

    std::thread cancel_thread([&]() {
        while (!start.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
        for (const OrderId id : order_ids)
        {
            const auto cancel = exchange.cancel_order(seller, id);
            if (!cancel.has_value() && cancel.error() != CancelOrderError::OrderNotFound)
            {
                ok.store(false, std::memory_order_release);
                break;
            }
        }
    });

    start.store(true, std::memory_order_release);
    market_thread.join();
    cancel_thread.join();

    ASSERT_TRUE(ok.load(std::memory_order_acquire));

    const auto seller_btc_free = exchange.free_balance(seller, btc);
    const auto seller_btc_reserved = exchange.reserved_balance(seller, btc);
    const auto seller_usdt_free = exchange.free_balance(seller, usdt);
    const auto buyer_btc_free = exchange.free_balance(buyer, btc);
    const auto buyer_usdt_free = exchange.free_balance(buyer, usdt);
    const auto buyer_usdt_reserved = exchange.reserved_balance(buyer, usdt);
    ASSERT_TRUE(seller_btc_free.has_value());
    ASSERT_TRUE(seller_btc_reserved.has_value());
    ASSERT_TRUE(seller_usdt_free.has_value());
    ASSERT_TRUE(buyer_btc_free.has_value());
    ASSERT_TRUE(buyer_usdt_free.has_value());
    ASSERT_TRUE(buyer_usdt_reserved.has_value());

    EXPECT_GE(*seller_btc_free, 0);
    EXPECT_GE(*seller_btc_reserved, 0);
    EXPECT_GE(*seller_usdt_free, 0);
    EXPECT_GE(*buyer_btc_free, 0);
    EXPECT_GE(*buyer_usdt_free, 0);
    EXPECT_GE(*buyer_usdt_reserved, 0);

    // No BTC/USDT should disappear between the two users.
    EXPECT_EQ(*seller_btc_free + *seller_btc_reserved + *buyer_btc_free, initial_seller_btc);
    EXPECT_EQ(*seller_usdt_free + *buyer_usdt_free + *buyer_usdt_reserved, initial_buyer_usdt);
}

TEST(ExchangeConcurrencyTest, PlaceLimitParallelOnTwoMarketsKeepsBalancesNonNegative)
{
    Exchange exchange;
    const Market btc_usdt_market = Market{Asset{"btc"}, Asset{"usdt"}};
    const Market eth_usdt_market = Market{Asset{"eth"}, Asset{"usdt"}};
    const Asset btc{"btc"};
    const Asset eth{"eth"};
    const Asset usdt{"usdt"};

    ASSERT_TRUE(exchange.register_market(btc_usdt_market).has_value());
    ASSERT_TRUE(exchange.register_market(eth_usdt_market).has_value());

    const auto btc_buyer_result = exchange.create_user("btc-buyer-parallel");
    const auto btc_seller_result = exchange.create_user("btc-seller-parallel");
    const auto eth_buyer_result = exchange.create_user("eth-buyer-parallel");
    const auto eth_seller_result = exchange.create_user("eth-seller-parallel");
    ASSERT_TRUE(btc_buyer_result.has_value());
    ASSERT_TRUE(btc_seller_result.has_value());
    ASSERT_TRUE(eth_buyer_result.has_value());
    ASSERT_TRUE(eth_seller_result.has_value());

    const UserId btc_buyer = *btc_buyer_result;
    const UserId btc_seller = *btc_seller_result;
    const UserId eth_buyer = *eth_buyer_result;
    const UserId eth_seller = *eth_seller_result;

    constexpr int kThreadsPerMarket = 4;
    constexpr int kOpsPerThread = 600;

    ASSERT_TRUE(exchange.deposit(btc_buyer, usdt, 1000000).has_value());
    ASSERT_TRUE(exchange.deposit(btc_seller, btc, 1000000).has_value());
    ASSERT_TRUE(exchange.deposit(eth_buyer, usdt, 1000000).has_value());
    ASSERT_TRUE(exchange.deposit(eth_seller, eth, 1000000).has_value());

    std::atomic<bool> start{false};
    std::atomic<bool> ok{true};
    std::vector<std::thread> threads;
    threads.reserve(kThreadsPerMarket * 2);

    for (int i = 0; i < kThreadsPerMarket; ++i)
    {
        threads.emplace_back([&]() {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
            for (int j = 0; j < kOpsPerThread; ++j)
            {
                const auto buy = exchange.place_limit_order(btc_buyer, btc_usdt_market, Side::Buy, 1, 1);
                if (!buy.has_value())
                {
                    ok.store(false, std::memory_order_release);
                    return;
                }
                const auto sell = exchange.place_limit_order(btc_seller, btc_usdt_market, Side::Sell, 1, 1);
                if (!sell.has_value())
                {
                    ok.store(false, std::memory_order_release);
                    return;
                }
            }
        });
    }

    for (int i = 0; i < kThreadsPerMarket; ++i)
    {
        threads.emplace_back([&]() {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
            for (int j = 0; j < kOpsPerThread; ++j)
            {
                const auto buy = exchange.place_limit_order(eth_buyer, eth_usdt_market, Side::Buy, 1, 1);
                if (!buy.has_value())
                {
                    ok.store(false, std::memory_order_release);
                    return;
                }
                const auto sell = exchange.place_limit_order(eth_seller, eth_usdt_market, Side::Sell, 1, 1);
                if (!sell.has_value())
                {
                    ok.store(false, std::memory_order_release);
                    return;
                }
            }
        });
    }

    start.store(true, std::memory_order_release);
    for (auto &thread : threads)
    {
        thread.join();
    }

    ASSERT_TRUE(ok.load(std::memory_order_acquire));

    const auto btc_buyer_btc_free = exchange.free_balance(btc_buyer, btc);
    const auto btc_buyer_usdt_free = exchange.free_balance(btc_buyer, usdt);
    const auto btc_buyer_usdt_reserved = exchange.reserved_balance(btc_buyer, usdt);
    const auto btc_seller_btc_free = exchange.free_balance(btc_seller, btc);
    const auto btc_seller_btc_reserved = exchange.reserved_balance(btc_seller, btc);
    const auto btc_seller_usdt_free = exchange.free_balance(btc_seller, usdt);

    const auto eth_buyer_eth_free = exchange.free_balance(eth_buyer, eth);
    const auto eth_buyer_usdt_free = exchange.free_balance(eth_buyer, usdt);
    const auto eth_buyer_usdt_reserved = exchange.reserved_balance(eth_buyer, usdt);
    const auto eth_seller_eth_free = exchange.free_balance(eth_seller, eth);
    const auto eth_seller_eth_reserved = exchange.reserved_balance(eth_seller, eth);
    const auto eth_seller_usdt_free = exchange.free_balance(eth_seller, usdt);

    ASSERT_TRUE(btc_buyer_btc_free.has_value());
    ASSERT_TRUE(btc_buyer_usdt_free.has_value());
    ASSERT_TRUE(btc_buyer_usdt_reserved.has_value());
    ASSERT_TRUE(btc_seller_btc_free.has_value());
    ASSERT_TRUE(btc_seller_btc_reserved.has_value());
    ASSERT_TRUE(btc_seller_usdt_free.has_value());
    ASSERT_TRUE(eth_buyer_eth_free.has_value());
    ASSERT_TRUE(eth_buyer_usdt_free.has_value());
    ASSERT_TRUE(eth_buyer_usdt_reserved.has_value());
    ASSERT_TRUE(eth_seller_eth_free.has_value());
    ASSERT_TRUE(eth_seller_eth_reserved.has_value());
    ASSERT_TRUE(eth_seller_usdt_free.has_value());

    EXPECT_GE(*btc_buyer_btc_free, 0);
    EXPECT_GE(*btc_buyer_usdt_free, 0);
    EXPECT_GE(*btc_buyer_usdt_reserved, 0);
    EXPECT_GE(*btc_seller_btc_free, 0);
    EXPECT_GE(*btc_seller_btc_reserved, 0);
    EXPECT_GE(*btc_seller_usdt_free, 0);
    EXPECT_GE(*eth_buyer_eth_free, 0);
    EXPECT_GE(*eth_buyer_usdt_free, 0);
    EXPECT_GE(*eth_buyer_usdt_reserved, 0);
    EXPECT_GE(*eth_seller_eth_free, 0);
    EXPECT_GE(*eth_seller_eth_reserved, 0);
    EXPECT_GE(*eth_seller_usdt_free, 0);

    expect_no_orphan_orders(exchange, {btc_usdt_market, eth_usdt_market});
}

TEST(ExchangeConcurrencyTest, CancelDuringHighInflowDoesNotDeadlockAndRestoresSellerReservation)
{
    Exchange exchange;
    const Market market = btc_usdt();
    const Asset btc{"btc"};

    ASSERT_TRUE(exchange.register_market(market).has_value());

    const auto seller_result = exchange.create_user("seller-inflow");
    ASSERT_TRUE(seller_result.has_value());
    const UserId seller = *seller_result;

    constexpr int kProducerThreads = 4;
    constexpr int kOrdersPerProducer = 500;
    constexpr int kTotalOrders = kProducerThreads * kOrdersPerProducer;
    ASSERT_TRUE(exchange.deposit(seller, btc, kTotalOrders + 100).has_value());

    std::atomic<bool> start{false};
    std::atomic<bool> producers_done{false};
    std::atomic<bool> ok{true};
    std::vector<OrderId> order_ids;
    order_ids.reserve(kTotalOrders);
    std::mutex order_ids_mu;
    std::size_t next_cancel_index = 0;

    TimeoutAbortGuard guard(std::chrono::milliseconds(8000));

    std::vector<std::thread> producers;
    producers.reserve(kProducerThreads);
    for (int t = 0; t < kProducerThreads; ++t)
    {
        producers.emplace_back([&]() {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            for (int i = 0; i < kOrdersPerProducer; ++i)
            {
                const auto place = exchange.place_limit_order(seller, market, Side::Sell, 1, 1);
                if (!place.has_value())
                {
                    ok.store(false, std::memory_order_release);
                    return;
                }

                std::lock_guard lock(order_ids_mu);
                order_ids.push_back(place->order_id);
            }
        });
    }

    std::thread cancel_thread([&]() {
        while (!start.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }

        while (true)
        {
            bool has_id = false;
            OrderId order_id;
            {
                std::lock_guard lock(order_ids_mu);
                if (next_cancel_index < order_ids.size())
                {
                    order_id = order_ids[next_cancel_index];
                    ++next_cancel_index;
                    has_id = true;
                }
                else if (producers_done.load(std::memory_order_acquire))
                {
                    return;
                }
            }

            if (!has_id)
            {
                std::this_thread::yield();
                continue;
            }

            const auto cancel = exchange.cancel_order(seller, order_id);
            if (!cancel.has_value() && cancel.error() != CancelOrderError::OrderNotFound)
            {
                ok.store(false, std::memory_order_release);
                return;
            }
        }
    });

    start.store(true, std::memory_order_release);
    for (auto &producer : producers)
    {
        producer.join();
    }
    producers_done.store(true, std::memory_order_release);
    cancel_thread.join();

    ASSERT_TRUE(ok.load(std::memory_order_acquire));

    const auto seller_btc_free = exchange.free_balance(seller, btc);
    const auto seller_btc_reserved = exchange.reserved_balance(seller, btc);
    ASSERT_TRUE(seller_btc_free.has_value());
    ASSERT_TRUE(seller_btc_reserved.has_value());
    EXPECT_GE(*seller_btc_free, 0);
    EXPECT_GE(*seller_btc_reserved, 0);
    EXPECT_EQ(*seller_btc_reserved, 0);

    expect_no_orphan_orders(exchange, {market});
}

TEST(ExchangeConcurrencyTest, SharedUsersAndDisjointUsersContentionScenariosStayConsistent)
{
    const Asset usdt{"usdt"};

    const auto run_scenario = [&](bool shared_users) {
        Exchange exchange;
        constexpr int kThreads = 8;
        constexpr int kIterations = 1200;
        constexpr int kInitialBalance = 5000;

        std::vector<UserId> users;
        if (shared_users)
        {
            const auto u1 = exchange.create_user("shared-a");
            const auto u2 = exchange.create_user("shared-b");
            ASSERT_TRUE(u1.has_value());
            ASSERT_TRUE(u2.has_value());
            users.push_back(*u1);
            users.push_back(*u2);
        }
        else
        {
            users.reserve(kThreads);
            for (int i = 0; i < kThreads; ++i)
            {
                const auto user = exchange.create_user("disjoint-" + std::to_string(i));
                ASSERT_TRUE(user.has_value());
                users.push_back(*user);
            }
        }

        for (const UserId user_id : users)
        {
            ASSERT_TRUE(exchange.deposit(user_id, usdt, kInitialBalance).has_value());
        }

        std::atomic<bool> start{false};
        std::atomic<bool> ok{true};
        std::vector<std::thread> threads;
        threads.reserve(kThreads);

        for (int t = 0; t < kThreads; ++t)
        {
            const UserId user_id = shared_users ? users[static_cast<std::size_t>(t % users.size())] : users[static_cast<std::size_t>(t)];
            threads.emplace_back([&exchange, &start, &ok, user_id, usdt]() {
                while (!start.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }
                for (int i = 0; i < kIterations; ++i)
                {
                    if (!exchange.deposit(user_id, usdt, 2).has_value())
                    {
                        ok.store(false, std::memory_order_release);
                        return;
                    }
                    if (!exchange.reserve(user_id, usdt, 1).has_value())
                    {
                        ok.store(false, std::memory_order_release);
                        return;
                    }
                    if (!exchange.release(user_id, usdt, 1).has_value())
                    {
                        ok.store(false, std::memory_order_release);
                        return;
                    }
                    if (!exchange.withdraw(user_id, usdt, 2).has_value())
                    {
                        ok.store(false, std::memory_order_release);
                        return;
                    }
                }
            });
        }

        start.store(true, std::memory_order_release);
        for (auto &thread : threads)
        {
            thread.join();
        }
        ASSERT_TRUE(ok.load(std::memory_order_acquire));

        for (const UserId user_id : users)
        {
            const auto free = exchange.free_balance(user_id, usdt);
            const auto reserved = exchange.reserved_balance(user_id, usdt);
            ASSERT_TRUE(free.has_value());
            ASSERT_TRUE(reserved.has_value());
            EXPECT_EQ(*free, kInitialBalance);
            EXPECT_EQ(*reserved, 0);
        }
    };

    run_scenario(true);
    run_scenario(false);
}

TEST(ExchangeConcurrencyTest, HighTrafficOnBtcUsdtDoesNotBreakEthUsdtFlow)
{
    Exchange exchange;
    const Market btc_usdt_market = Market{Asset{"btc"}, Asset{"usdt"}};
    const Market eth_usdt_market = Market{Asset{"eth"}, Asset{"usdt"}};
    const Asset btc{"btc"};
    const Asset eth{"eth"};
    const Asset usdt{"usdt"};

    ASSERT_TRUE(exchange.register_market(btc_usdt_market).has_value());
    ASSERT_TRUE(exchange.register_market(eth_usdt_market).has_value());

    const auto btc_seller_result = exchange.create_user("btc-seller-iso");
    const auto btc_buyer_result = exchange.create_user("btc-buyer-iso");
    const auto eth_seller_result = exchange.create_user("eth-seller-iso");
    const auto eth_buyer_result = exchange.create_user("eth-buyer-iso");
    ASSERT_TRUE(btc_seller_result.has_value());
    ASSERT_TRUE(btc_buyer_result.has_value());
    ASSERT_TRUE(eth_seller_result.has_value());
    ASSERT_TRUE(eth_buyer_result.has_value());

    const UserId btc_seller = *btc_seller_result;
    const UserId btc_buyer = *btc_buyer_result;
    const UserId eth_seller = *eth_seller_result;
    const UserId eth_buyer = *eth_buyer_result;

    constexpr int kBtcOps = 1200;
    constexpr int kEthOrders = 1200;

    ASSERT_TRUE(exchange.deposit(btc_seller, btc, kBtcOps + 100).has_value());
    ASSERT_TRUE(exchange.deposit(btc_buyer, usdt, kBtcOps + 100).has_value());
    ASSERT_TRUE(exchange.deposit(eth_seller, eth, kEthOrders).has_value());
    ASSERT_TRUE(exchange.deposit(eth_buyer, usdt, kEthOrders).has_value());

    for (int i = 0; i < kEthOrders; ++i)
    {
        const auto place = exchange.place_limit_order(eth_seller, eth_usdt_market, Side::Sell, 1, 1);
        ASSERT_TRUE(place.has_value());
    }

    std::atomic<bool> btc_ok{true};
    TimeoutAbortGuard guard(std::chrono::milliseconds(8000));

    std::thread btc_traffic([&]() {
        for (int i = 0; i < kBtcOps; ++i)
        {
            const auto place = exchange.place_limit_order(btc_seller, btc_usdt_market, Side::Sell, 1, 1);
            if (!place.has_value())
            {
                btc_ok.store(false, std::memory_order_release);
                return;
            }

            const auto buy = exchange.execute_market_order(btc_buyer, btc_usdt_market, Side::Buy, 1);
            if (!buy.has_value())
            {
                btc_ok.store(false, std::memory_order_release);
                return;
            }
        }
    });

    for (int i = 0; i < kEthOrders; ++i)
    {
        const auto buy = exchange.execute_market_order(eth_buyer, eth_usdt_market, Side::Buy, 1);
        ASSERT_TRUE(buy.has_value());
    }

    btc_traffic.join();
    ASSERT_TRUE(btc_ok.load(std::memory_order_acquire));

    const auto eth_seller_eth_free = exchange.free_balance(eth_seller, eth);
    const auto eth_seller_eth_reserved = exchange.reserved_balance(eth_seller, eth);
    const auto eth_seller_usdt_free = exchange.free_balance(eth_seller, usdt);
    const auto eth_buyer_eth_free = exchange.free_balance(eth_buyer, eth);
    const auto eth_buyer_usdt_free = exchange.free_balance(eth_buyer, usdt);
    const auto eth_buyer_usdt_reserved = exchange.reserved_balance(eth_buyer, usdt);

    ASSERT_TRUE(eth_seller_eth_free.has_value());
    ASSERT_TRUE(eth_seller_eth_reserved.has_value());
    ASSERT_TRUE(eth_seller_usdt_free.has_value());
    ASSERT_TRUE(eth_buyer_eth_free.has_value());
    ASSERT_TRUE(eth_buyer_usdt_free.has_value());
    ASSERT_TRUE(eth_buyer_usdt_reserved.has_value());

    EXPECT_EQ(*eth_seller_eth_free, 0);
    EXPECT_EQ(*eth_seller_eth_reserved, 0);
    EXPECT_EQ(*eth_seller_usdt_free, kEthOrders);
    EXPECT_EQ(*eth_buyer_eth_free, kEthOrders);
    EXPECT_EQ(*eth_buyer_usdt_free, 0);
    EXPECT_EQ(*eth_buyer_usdt_reserved, 0);

    expect_no_orphan_orders(exchange, {btc_usdt_market, eth_usdt_market});
}
