#pragma once

#include <chrono>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "vertex/application/exchange.hpp"
#include "vertex/core/strong_id.hpp"

using Exchange = vertex::application::Exchange;
using Market = vertex::application::Market;
using Asset = vertex::application::Asset;
using UserId = vertex::core::UserId;
using Side = vertex::core::Side;

struct BenchConfig
{
    int warmup_seconds;
    int measure_seconds;
    int repeats;
    int thread_count;
    std::uint32_t seed;
    bool verbose;
};

enum class ScenarioKind
{
    SingleMarketHighLoad,
    MultiMarketParallelLoad,
    SharedUsersContention,
    DisjointUsersContention
};

struct CliArgs
{
    BenchConfig config;
    std::vector<ScenarioKind> scenarios;
};

struct LatencyStats
{
    double p50_us;
    double p95_us;
    double p99_us;
};

struct ThroughputStats
{
    double ops_per_sec;
    std::uint64_t total_ops;
};

struct ScenarioMetrics
{
    ScenarioKind scenario;
    int repeat_index;
    ThroughputStats throughput;
    LatencyStats latency;
};

struct AggregateMetrics
{
    ScenarioKind scenario;
    double median_ops_per_sec;
    double median_p50_us;
    double median_p95_us;
    double median_p99_us;
};

enum class OpKind
{
    PlaceLimitBuy,
    PlaceLimitSell,
    MarketBuy,
    MarketSell,
    Cancel
};

struct OpSample
{
    OpKind op;
    std::chrono::nanoseconds latency;
    bool success;
};

class BenchmarkRunner
{
public:
    explicit BenchmarkRunner(BenchConfig cfg);

    std::vector<ScenarioMetrics> run_scenario(ScenarioKind kind);
    AggregateMetrics aggregate(ScenarioKind kind, const std::vector<ScenarioMetrics> &runs) const;
    ScenarioMetrics run_single_market(int repeat_index);
    ScenarioMetrics run_multi_market(int repeat_index);
    ScenarioMetrics run_disjoint_users(int repeat_index);
    ScenarioMetrics run_shared_users(int repeat_index);

private:
    BenchConfig cfg_;


    std::mt19937 make_thread_rng(int repeat_index, int thread_index) const;
    OpKind pick_random_op(std::mt19937 &rng) const;
    UserId pick_random_user(std::mt19937 &rng, const std::vector<UserId> &users) const;
    bool execute_one_op(std::mt19937 &rng, Exchange &ex, const Market &market, const UserId buyer, const UserId seller, OpKind op);
};

