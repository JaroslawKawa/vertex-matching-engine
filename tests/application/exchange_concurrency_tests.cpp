#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>

#include "vertex/application/exchange.hpp"
#include "vertex/application/trade_history.hpp"

namespace
{
    using vertex::application::Exchange;
    using vertex::application::TradeHistory;
    using vertex::core::Asset;
    using vertex::core::Market;
    using vertex::core::OrderId;
    using vertex::core::TradeId;
    using vertex::core::UserId;
    using vertex::domain::Trade;

    Market btc_usdt()
    {
        return Market{Asset{"btc"}, Asset{"usdt"}};
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
