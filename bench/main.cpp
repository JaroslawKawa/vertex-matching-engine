#include <iostream>
#include "benchmark_runner.hpp"

void print_run_result(const ScenarioMetrics &m);
void print_aggregate_result(const AggregateMetrics &m);
std::string to_string(ScenarioKind s);
std::string to_string(OpKind op);

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    BenchConfig cfg;
    cfg.warmup_seconds = 2;
    cfg.measure_seconds = 10;
    cfg.repeats = 10;
    cfg.thread_count = 8;
    cfg.seed = 0xC0FFEEu;
    cfg.verbose = true;

    BenchmarkRunner run{cfg};
    print_run_result(run.run_single_market(1));

    return 0;
}

void print_run_result(const ScenarioMetrics &m)
{
    auto print = std::format("[{}][Index={}][Ops per sec={}, Total ops={}][Latency: p50={}, p95={}, p99={}]\n",
                             to_string(m.scenario),
                             m.repeat_index,
                             m.throughput.ops_per_sec,
                             m.throughput.total_ops,
                             m.latency.p50_us,
                             m.latency.p95_us,
                             m.latency.p99_us);

    std::cout << print;
}

void print_aggregate_result(const AggregateMetrics &m)
{
    auto print = std::format("[{}][Ops per sec mediana={}][Latency median: p50={}, p95={}, p99={}]",
                             to_string(m.scenario),
                             m.median_ops_per_sec,
                             m.median_p50_us,
                             m.median_p95_us,
                             m.median_p99_us);

    std::cout << print;
}

std::string to_string(ScenarioKind s)
{
    switch (s)
    {
    case ScenarioKind::SingleMarketHighLoad:
        return "SingleMarketHighLoad";
    case ScenarioKind::MultiMarketParallelLoad:
        return "MultiMarketParallelLoad";
    case ScenarioKind::SharedUsersContention:
        return "SharedUsersContention";
    case ScenarioKind::DisjointUsersContention:
        return "DisjointUsersContention";
    default:
        return "Invalid scenario kind";
    }
}

std::string to_string(OpKind op)
{
    switch (op)
    {
    case OpKind::PlaceLimitBuy:
        return "PlaceLimitBuy";
    case OpKind::PlaceLimitSell:
        return "PlaceLimitSell";
    case OpKind::MarketBuy:
        return "MarketBuy";
    case OpKind::MarketSell:
        return "MarketSell";
    case OpKind::Cancel:
        return "Cancel";
    default:
        return "Invalid operation kind";
    }
}
