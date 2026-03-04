#include <algorithm>
#include <charconv>
#include <cstdint>
#include <format>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "benchmark_runner.hpp"

namespace
{
    struct CliArgs
    {
        BenchConfig config;
        std::vector<ScenarioKind> scenarios;
    };

    struct ParseResult
    {
        bool ok;
        bool help_requested;
        CliArgs args;
        std::string error;
    };

    BenchConfig default_bench_config()
    {
        BenchConfig cfg;
        cfg.warmup_seconds = 2;
        cfg.measure_seconds = 10;
        cfg.repeats = 10;
        cfg.thread_count = 24;
        cfg.seed = 0xC0FFEEu;
        cfg.verbose = true;
        return cfg;
    }

    std::vector<ScenarioKind> default_scenarios()
    {
        return {
            ScenarioKind::SingleMarketHighLoad,
            ScenarioKind::MultiMarketParallelLoad,
            ScenarioKind::DisjointUsersContention,
            ScenarioKind::SharedUsersContention};
    }

    std::string to_lower(std::string_view text)
    {
        std::string lowered(text);
        for (char &c : lowered)
        {
            if ('A' <= c && c <= 'Z')
            {
                c = static_cast<char>(c - 'A' + 'a');
            }
        }
        return lowered;
    }

    bool parse_int(std::string_view value, int &out)
    {
        int parsed = 0;
        const auto *begin = value.data();
        const auto *end = value.data() + value.size();
        auto [ptr, ec] = std::from_chars(begin, end, parsed);
        if (ec != std::errc() || ptr != end)
        {
            return false;
        }
        out = parsed;
        return true;
    }

    bool parse_u32(std::string_view value, std::uint32_t &out)
    {
        std::uint32_t parsed = 0;
        const auto *begin = value.data();
        const auto *end = value.data() + value.size();
        auto [ptr, ec] = std::from_chars(begin, end, parsed);
        if (ec != std::errc() || ptr != end)
        {
            return false;
        }
        out = parsed;
        return true;
    }

    std::optional<ScenarioKind> parse_scenario_token(std::string_view token)
    {
        const std::string value = to_lower(token);
        if (value == "single" || value == "single-market" || value == "single_market")
        {
            return ScenarioKind::SingleMarketHighLoad;
        }
        if (value == "multi" || value == "multi-market" || value == "multi_market")
        {
            return ScenarioKind::MultiMarketParallelLoad;
        }
        if (value == "disjoint" || value == "disjoint-users" || value == "disjoint_users")
        {
            return ScenarioKind::DisjointUsersContention;
        }
        if (value == "shared" || value == "shared-users" || value == "shared_users")
        {
            return ScenarioKind::SharedUsersContention;
        }
        return std::nullopt;
    }

    void deduplicate_scenarios(std::vector<ScenarioKind> &scenarios)
    {
        std::vector<ScenarioKind> unique;
        unique.reserve(scenarios.size());
        for (ScenarioKind s : scenarios)
        {
            if (std::find(unique.begin(), unique.end(), s) == unique.end())
            {
                unique.push_back(s);
            }
        }
        scenarios = std::move(unique);
    }

    void print_help(std::ostream &out)
    {
        out << "vertex_bench options:\n";
        out << "  --scenario <name|list>   single|multi|disjoint|shared|all (comma-separated)\n";
        out << "  --threads <int>          worker thread count (>0)\n";
        out << "  --warmup <int>           warmup seconds (>=0)\n";
        out << "  --measure <int>          measure seconds (>0)\n";
        out << "  --repeats <int>          repeats per scenario (>0)\n";
        out << "  --seed <uint32>          random seed\n";
        out << "  --verbose                print every run + aggregate\n";
        out << "  --quiet                  print aggregate only\n";
        out << "  --help                   show this help\n";
    }

    ParseResult parse_args(int argc, char **argv)
    {
        ParseResult result;
        result.ok = true;
        result.help_requested = false;
        result.args = CliArgs{
            .config = default_bench_config(),
            .scenarios = default_scenarios()};

        bool scenario_overridden = false;

        for (int i = 1; i < argc; ++i)
        {
            const std::string_view arg = argv[i];

            auto need_value = [&](std::string_view option) -> std::optional<std::string_view>
            {
                if (i + 1 >= argc)
                {
                    result.ok = false;
                    result.error = std::format("Missing value for option '{}'.", option);
                    return std::nullopt;
                }
                return std::string_view(argv[++i]);
            };

            if (arg == "--help" || arg == "-h")
            {
                result.help_requested = true;
                return result;
            }

            if (arg == "--verbose")
            {
                result.args.config.verbose = true;
                continue;
            }

            if (arg == "--quiet")
            {
                result.args.config.verbose = false;
                continue;
            }

            if (arg == "--scenario")
            {
                const auto value = need_value(arg);
                if (!value.has_value())
                {
                    return result;
                }

                if (!scenario_overridden)
                {
                    result.args.scenarios.clear();
                    scenario_overridden = true;
                }

                std::size_t start = 0;
                while (start <= value->size())
                {
                    const std::size_t comma = value->find(',', start);
                    const std::size_t end = (comma == std::string_view::npos) ? value->size() : comma;
                    const std::string_view token = value->substr(start, end - start);
                    const std::string lowered = to_lower(token);

                    if (lowered == "all")
                    {
                        const auto all = default_scenarios();
                        result.args.scenarios.insert(result.args.scenarios.end(), all.begin(), all.end());
                    }
                    else
                    {
                        const auto scenario = parse_scenario_token(token);
                        if (!scenario.has_value())
                        {
                            result.ok = false;
                            result.error = std::format("Unknown scenario '{}'.", token);
                            return result;
                        }
                        result.args.scenarios.push_back(*scenario);
                    }

                    if (comma == std::string_view::npos)
                    {
                        break;
                    }
                    start = comma + 1;
                }
                continue;
            }

            if (arg == "--threads")
            {
                const auto value = need_value(arg);
                if (!value.has_value())
                {
                    return result;
                }
                int parsed = 0;
                if (!parse_int(*value, parsed) || parsed <= 0)
                {
                    result.ok = false;
                    result.error = std::format("Invalid --threads value '{}'.", *value);
                    return result;
                }
                result.args.config.thread_count = parsed;
                continue;
            }

            if (arg == "--warmup")
            {
                const auto value = need_value(arg);
                if (!value.has_value())
                {
                    return result;
                }
                int parsed = 0;
                if (!parse_int(*value, parsed) || parsed < 0)
                {
                    result.ok = false;
                    result.error = std::format("Invalid --warmup value '{}'.", *value);
                    return result;
                }
                result.args.config.warmup_seconds = parsed;
                continue;
            }

            if (arg == "--measure")
            {
                const auto value = need_value(arg);
                if (!value.has_value())
                {
                    return result;
                }
                int parsed = 0;
                if (!parse_int(*value, parsed) || parsed <= 0)
                {
                    result.ok = false;
                    result.error = std::format("Invalid --measure value '{}'.", *value);
                    return result;
                }
                result.args.config.measure_seconds = parsed;
                continue;
            }

            if (arg == "--repeats")
            {
                const auto value = need_value(arg);
                if (!value.has_value())
                {
                    return result;
                }
                int parsed = 0;
                if (!parse_int(*value, parsed) || parsed <= 0)
                {
                    result.ok = false;
                    result.error = std::format("Invalid --repeats value '{}'.", *value);
                    return result;
                }
                result.args.config.repeats = parsed;
                continue;
            }

            if (arg == "--seed")
            {
                const auto value = need_value(arg);
                if (!value.has_value())
                {
                    return result;
                }
                std::uint32_t parsed = 0;
                if (!parse_u32(*value, parsed))
                {
                    result.ok = false;
                    result.error = std::format("Invalid --seed value '{}'.", *value);
                    return result;
                }
                result.args.config.seed = parsed;
                continue;
            }

            result.ok = false;
            result.error = std::format("Unknown option '{}'.", arg);
            return result;
        }

        deduplicate_scenarios(result.args.scenarios);
        if (result.args.scenarios.empty())
        {
            result.ok = false;
            result.error = "No scenarios selected.";
            return result;
        }

        return result;
    }
}

void print_run_result(const ScenarioMetrics &m);
void print_aggregate_result(const AggregateMetrics &m);
std::string to_string(ScenarioKind s);
std::string to_string(OpKind op);

int main(int argc, char **argv)
{
    const ParseResult parsed = parse_args(argc, argv);
    if (parsed.help_requested)
    {
        print_help(std::cout);
        return 0;
    }

    if (!parsed.ok)
    {
        std::cerr << "Argument error: " << parsed.error << '\n';
        print_help(std::cerr);
        return 1;
    }

    BenchmarkRunner run{parsed.args.config};
    for (ScenarioKind scenario : parsed.args.scenarios)
    {
        const std::vector<ScenarioMetrics> runs = run.run_scenario(scenario);
        if (parsed.args.config.verbose)
        {
            for (const auto &single_run : runs)
            {
                print_run_result(single_run);
            }
        }
        print_aggregate_result(run.aggregate(scenario, runs));
    }

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
    auto print = std::format("[{}][Ops per sec mediana={}][Latency median: p50={}, p95={}, p99={}]\n",
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
