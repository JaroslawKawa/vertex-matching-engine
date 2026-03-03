#include "benchmark_runner.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>
#include <iostream>
#include <latch>
#include <random>
#include <string>
#include <thread>
#include <vector>

using SteadyClock = std::chrono::steady_clock;

BenchmarkRunner::BenchmarkRunner(BenchConfig cfg) : cfg_(cfg) {}

ScenarioMetrics BenchmarkRunner::run_single_market(int repeat_index)
{
    Exchange exchange;
    Asset btc{"BTC"};
    Asset usdt{"USDT"};

    Market market(btc, usdt);
    exchange.register_market(market);

    std::vector<UserId> buyers;
    buyers.reserve(cfg_.thread_count / 2);
    std::vector<UserId> sellers;
    sellers.reserve(cfg_.thread_count / 2);

    for (int i = 0; i < cfg_.thread_count / 2; ++i)
    {
        char c = static_cast<char>('A' + i);
        std::string name(1, c);
        UserId buyer_user = exchange.create_user(std::string("Buyer" + name)).value();
        exchange.deposit(buyer_user, usdt, 1'000'000);
        buyers.push_back(buyer_user);

        UserId seller_user = exchange.create_user(std::string("Seller" + name)).value();
        exchange.deposit(seller_user, btc, 1'000'000);
        sellers.push_back(seller_user);
    }

    std::latch start_latch(cfg_.thread_count);
    std::atomic<bool> measuring{false};
    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> measured_ops{0};

    std::vector<std::vector<double>> lat_us_per_thread(cfg_.thread_count);

    std::vector<std::thread> workers;
    workers.reserve(cfg_.thread_count);

    for (int tid = 0; tid < cfg_.thread_count; ++tid)
    {
        workers.emplace_back([&, tid]
                             {
            std::mt19937 rng = make_thread_rng(repeat_index, tid);
            auto& local_lat = lat_us_per_thread[tid];
            local_lat.reserve(10000);

            start_latch.count_down();
            start_latch.wait();

            while(!stop.load(std::memory_order_acquire)){
                OpKind op = pick_random_op(rng);

                auto t0 = SteadyClock::now();
                bool ok = execute_one_op(exchange, market, buyers, sellers, op);
                auto t1 = SteadyClock::now();

                if(ok && measuring.load(std::memory_order_relaxed)){
                    measured_ops.fetch_add(1, std::memory_order_relaxed);
                    double us = std::chrono::duration<double,std::micro>(t1 - t0).count();
                    local_lat.push_back(us);
                }
            } });
    }

    std::this_thread::sleep_for(std::chrono::seconds(cfg_.warmup_seconds));

    measuring.store(true, std::memory_order_release);
    auto measure_start = SteadyClock::now();

    std::this_thread::sleep_for(std::chrono::seconds(cfg_.measure_seconds));

    auto measure_stop = SteadyClock::now();
    measuring.store(false, std::memory_order_release);

    stop.store(true, std::memory_order_release);

    for (auto &th : workers)
    {
        th.join();
    }

    std::vector<double> all_lat_us;
    for (const auto &th_lat : lat_us_per_thread)
    {
        for (double lat : th_lat)
        {
            all_lat_us.push_back(lat);
        }
    }

    std::sort(all_lat_us.begin(), all_lat_us.end());

    auto pct = [&](double p) -> double
    {
        if (all_lat_us.empty())
        {
            return 0.0;
        }
        const std::size_t idx = static_cast<std::size_t>(std::floor((p / 100.0) * (all_lat_us.size() - 1)));
        return all_lat_us[idx];
    };

    double measured_s = std::chrono::duration<double>(measure_stop - measure_start).count();
    std::uint64_t ops = measured_ops.load(std::memory_order_relaxed);
    double ops_per_sec = measured_s > 0 ? static_cast<double>(ops) / measured_s : 0.0;

    return ScenarioMetrics{
        .scenario = ScenarioKind::SingleMarketHighLoad,
        .repeat_index = repeat_index,
        .throughput = ThroughputStats{
            .ops_per_sec = ops_per_sec,
            .total_ops = ops},
        .latency = LatencyStats{
            .p50_us = pct(50),
            .p95_us = pct(95),
            .p99_us = pct(99)}};
}

std::mt19937 BenchmarkRunner::make_thread_rng(int repeat_index, int thread_index) const
{
    const auto stream = static_cast<std::uint32_t>(repeat_index * 1000 + thread_index);
    return std::mt19937(cfg_.seed + stream);
}

OpKind BenchmarkRunner::pick_random_op(std::mt19937 &rng) const
{
    std::uniform_int_distribution<int> dist(1, 100);
    const int draw = dist(rng);

    if (draw <= 30)
        return OpKind::PlaceLimitBuy;
    if (draw <= 60)
        return OpKind::PlaceLimitSell;
    if (draw <= 85)
        return OpKind::MarketBuy;
    if (draw <= 95)
        return OpKind::MarketSell;
    return OpKind::Cancel;
}

bool BenchmarkRunner::execute_one_op(
    Exchange &ex,
    const Market &market,
    const std::vector<UserId> &buyers,
    const std::vector<UserId> &sellers,
    OpKind op)
{
    if (buyers.empty() || sellers.empty())
    {
        return false;
    }

    static thread_local std::mt19937 rng(0xC0FFEEu);
    std::uniform_int_distribution<std::size_t> buyer_dist(0, buyers.size() - 1);
    std::uniform_int_distribution<std::size_t> seller_dist(0, sellers.size() - 1);
    std::uniform_int_distribution<int> side_dist(0, 1);

    const UserId buyer = buyers[buyer_dist(rng)];
    const UserId seller = sellers[seller_dist(rng)];

    switch (op)
    {
    case OpKind::PlaceLimitBuy:
        return ex.place_limit_order(buyer, market, Side::Buy, 1, 1).has_value();
    case OpKind::PlaceLimitSell:
        return ex.place_limit_order(seller, market, Side::Sell, 1, 1).has_value();
    case OpKind::MarketBuy:
        return ex.execute_market_order(buyer, market, Side::Buy, 1).has_value();
    case OpKind::MarketSell:
        return ex.execute_market_order(seller, market, Side::Sell, 1).has_value();
    case OpKind::Cancel:
    {
        if (side_dist(rng) == 0)
        {
            auto placed = ex.place_limit_order(buyer, market, Side::Buy, 1, 1);
            return placed.has_value() && ex.cancel_order(buyer, placed->order_id).has_value();
        }
        auto placed = ex.place_limit_order(seller, market, Side::Sell, 1, 1);
        return placed.has_value() && ex.cancel_order(seller, placed->order_id).has_value();
    }
    default:
        return false;
    }
}
