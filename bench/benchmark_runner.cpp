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
namespace
{
    double median(std::vector<double> values)
    {
        if (values.empty())
        {
            return 0.0;
        }

        std::sort(values.begin(), values.end());
        const std::size_t n = values.size();
        const std::size_t mid = n / 2;

        if (n % 2 == 1)
        {
            return values[mid];
        }

        return (values[mid - 1] + values[mid]) * 0.5;
    }

}

BenchmarkRunner::BenchmarkRunner(BenchConfig cfg) : cfg_(cfg) {}

std::vector<ScenarioMetrics> BenchmarkRunner::run_scenario(ScenarioKind kind)
{
    std::vector<ScenarioMetrics> result;
    result.reserve(cfg_.repeats);
    switch (kind)
    {
    case ScenarioKind::SingleMarketHighLoad:
        for (int i = 0; i < cfg_.repeats; ++i)
        {
            result.push_back(run_single_market(i));
        }
        return result;

    case ScenarioKind::MultiMarketParallelLoad:
        for (int i = 0; i < cfg_.repeats; ++i)
        {
            result.push_back(run_multi_market(i));
        }
        return result;

    case ScenarioKind::SharedUsersContention:
        for (int i = 0; i < cfg_.repeats; ++i)
        {
            result.push_back(run_shared_users(i));
        }
        return result;

    case ScenarioKind::DisjointUsersContention:
        for (int i = 0; i < cfg_.repeats; ++i)
        {
            result.push_back(run_disjoint_users(i));
        }
        return result;

    default:
    assert(false);
        return result;
    }
}

AggregateMetrics BenchmarkRunner::aggregate(ScenarioKind kind, const std::vector<ScenarioMetrics> &runs) const
{
    std::vector<double> ops_per_sec;
    ops_per_sec.reserve(runs.size());
    std::vector<double> p50;
    p50.reserve(runs.size());
    std::vector<double> p95;
    p95.reserve(runs.size());
    std::vector<double> p99;
    p99.reserve(runs.size());

    for (const auto &metric : runs)
    {
        ops_per_sec.push_back(metric.throughput.ops_per_sec);
        p50.push_back(metric.latency.p50_us);
        p95.push_back(metric.latency.p95_us);
        p99.push_back(metric.latency.p99_us);
    }

    return AggregateMetrics{
        .scenario = kind,
        .median_ops_per_sec = median(std::move(ops_per_sec)),
        .median_p50_us = median(std::move(p50)),
        .median_p95_us = median(std::move(p95)),
        .median_p99_us = median(std::move(p99)),
    };
}

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
        UserId buyer_user = exchange.create_user(std::string(std::format("Buyer_U{}", i))).value();
        exchange.deposit(buyer_user, usdt, 1'000'000);
        buyers.push_back(buyer_user);

        UserId seller_user = exchange.create_user(std::string(std::format("Seller_U{}", i))).value();
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
                UserId buyer = pick_random_user(rng, buyers);
                UserId seller = pick_random_user(rng, sellers);
                auto t0 = SteadyClock::now();
                bool ok = execute_one_op(rng, exchange, market, buyer, seller, op);
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
        .latency = LatencyStats{.p50_us = pct(50), .p95_us = pct(95), .p99_us = pct(99)}};
}

ScenarioMetrics BenchmarkRunner::run_multi_market(int repeat_index)
{
    auto result = run_disjoint_users(repeat_index);
    result.scenario = ScenarioKind::MultiMarketParallelLoad;
    return result;
}

ScenarioMetrics BenchmarkRunner::run_disjoint_users(int repeat_index)
{
    Exchange exchange;
    Asset btc{"BTC"};
    Asset usdt{"USDT"};
    Asset eth{"ETH"};
    Asset pln{"PLN"};
    Asset sol{"SOL"};

    const std::vector<Market> markets{{usdt, btc}, {eth, btc}, {pln, btc}, {sol, btc}};
    const std::size_t users_per_market = std::max<std::size_t>(4, 2 * static_cast<std::size_t>(cfg_.thread_count) / markets.size());

    for (auto &m : markets)
    {
        exchange.register_market(m);
    }

    std::vector<std::vector<UserId>> buyers(markets.size());
    std::vector<std::vector<UserId>> sellers(markets.size());
    for (std::size_t i = 0; i < markets.size(); ++i)
    {
        const Market market = markets[i];

        for (std::size_t j = 0; j < users_per_market; ++j)
        {
            UserId buyer_user = exchange.create_user(std::string(std::format("Buyer_M{}_U{}", i, j))).value();
            UserId seller_user = exchange.create_user(std::string(std::format("Seller_M{}_U{}", i, j))).value();
            exchange.deposit(buyer_user, market.quote(), 1'000'000);
            exchange.deposit(seller_user, market.base(), 1'000'000);
            buyers[i].push_back(buyer_user);
            sellers[i].push_back(seller_user);
        }
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
            size_t market_index = tid%markets.size();
            Market market = markets[market_index];
            const std::vector<UserId> &market_buyers = buyers[market_index];
            const std::vector<UserId> &market_sellers = sellers[market_index];
            start_latch.count_down();
            start_latch.wait();

            while(!stop.load(std::memory_order_acquire)){
                OpKind op = pick_random_op(rng);
                UserId buyer = pick_random_user(rng,market_buyers);
                UserId seller = pick_random_user(rng,market_sellers);
                auto t0 = SteadyClock::now();
                bool ok = execute_one_op(rng, exchange, market, buyer, seller, op);
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
        .scenario = ScenarioKind::DisjointUsersContention,
        .repeat_index = repeat_index,
        .throughput = ThroughputStats{
            .ops_per_sec = ops_per_sec,
            .total_ops = ops},
        .latency = LatencyStats{.p50_us = pct(50), .p95_us = pct(95), .p99_us = pct(99)}};
}

ScenarioMetrics BenchmarkRunner::run_shared_users(int repeat_index)
{
    Exchange exchange;
    Asset btc{"BTC"};
    Asset usdt{"USDT"};
    Asset eth{"ETH"};
    Asset pln{"PLN"};
    Asset sol{"SOL"};

    const std::vector<Market> markets{{usdt, btc}, {eth, btc}, {pln, btc}, {sol, btc}};
    const std::size_t users_per_market = std::max<std::size_t>(4, 2 * static_cast<std::size_t>(cfg_.thread_count) / markets.size());

    for (auto &m : markets)
    {
        exchange.register_market(m);
    }

    std::vector<UserId> buyers;
    buyers.reserve(markets.size() * users_per_market);
    std::vector<UserId> sellers;
    sellers.reserve(markets.size() * users_per_market);

    for (std::size_t i = 0; i < (markets.size() * users_per_market); ++i)
    {

        UserId buyer = exchange.create_user(std::format("Buyer_U{}", i)).value();
        UserId seller = exchange.create_user(std::format("Seller_U{}", i)).value();

        for (const auto &m : markets)
        {
            exchange.deposit(buyer, m.quote(), 1'000'000);
            exchange.deposit(seller, m.base(), 1'000'000);
        }
        buyers.push_back(buyer);
        sellers.push_back(seller);
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
            size_t market_index = tid%markets.size();
            Market market = markets[market_index];
            start_latch.count_down();
            start_latch.wait();

            while(!stop.load(std::memory_order_acquire)){
                OpKind op = pick_random_op(rng);
                UserId buyer = pick_random_user(rng,buyers);
                UserId seller = pick_random_user(rng,sellers);
                auto t0 = SteadyClock::now();
                bool ok = execute_one_op(rng, exchange, market, buyer, seller, op);
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
        .scenario = ScenarioKind::SharedUsersContention,
        .repeat_index = repeat_index,
        .throughput = ThroughputStats{
            .ops_per_sec = ops_per_sec,
            .total_ops = ops},
        .latency = LatencyStats{.p50_us = pct(50), .p95_us = pct(95), .p99_us = pct(99)}};
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

UserId BenchmarkRunner::pick_random_user(std::mt19937 &rng, const std::vector<UserId> &users) const
{

    assert(!users.empty());

    std::uniform_int_distribution<std::size_t> users_dist(0, users.size() - 1);

    return users[users_dist(rng)];
}

bool BenchmarkRunner::execute_one_op(std::mt19937 &rng, Exchange &ex, const Market &market, const UserId buyer, const UserId seller, OpKind op)
{
    std::uniform_int_distribution<int> side_dist(0, 1);

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
