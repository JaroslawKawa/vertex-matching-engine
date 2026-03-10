#include "vertex/application/order_analytics.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <limits>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vertex::application
{
    namespace
    {
        bool add_overflow_i64(std::int64_t a, std::int64_t b)
        {
            using Limits = std::numeric_limits<std::int64_t>;
            if (b > 0)
                return a > Limits::max() - b;
            if (b < 0)
                return a < Limits::min() - b;
            return false;
        }
    } // namespace

    std::size_t count_by_status(std::span<const OrderRecord> orders, OrderStatus status)
    {
        return std::ranges::count(orders, status, &OrderRecord::status);
    }

    std::size_t count_by_side(std::span<const OrderRecord> orders, Side side)
    {
        return std::ranges::count(orders, side, &OrderRecord::side);
    }

    Quantity total_executed_base(std::span<const OrderRecord> orders)
    {
        Quantity sum{0};

        for (const OrderRecord &order : orders)
        {
            assert(!add_overflow_i64(sum, order.executed_base_qty));
            sum += order.executed_base_qty;
        }

        return sum;
    }

    Quantity total_executed_quote(std::span<const OrderRecord> orders)
    {
        Quantity sum{0};

        for (const OrderRecord &order : orders)
        {
            assert(!add_overflow_i64(sum, order.executed_quote_qty));
            sum += order.executed_quote_qty;
        }

        return sum;
    }

    std::optional<double> average_fill_count(std::span<const OrderRecord> orders)
    {
        if (orders.empty())
            return std::nullopt;

        std::size_t fill_count{0};
        for (const OrderRecord &order : orders)
        {
            fill_count += order.fill_count;
        }

        return static_cast<double>(fill_count) / orders.size();
    }

    std::optional<double> completion_ratio(std::span<const OrderRecord> orders)
    {
        if (orders.empty())
            return std::nullopt;

        return static_cast<double>(count_by_status(orders, OrderStatus::Filled)) / orders.size();
    }

    std::optional<double> avg_order_notional(std::span<const OrderRecord> orders)
    {
        if (orders.empty())
            return std::nullopt;

        const Quantity sum_quote = total_executed_quote(orders);
        if (sum_quote == 0)
            return std::nullopt;

        return static_cast<double>(sum_quote) / orders.size();
    }

    std::optional<double> vwap_from_orders(std::span<const OrderRecord> orders)
    {
        if (orders.empty())
            return std::nullopt;

        const Quantity sum_quote = total_executed_quote(orders);
        if (sum_quote == 0)
            return std::nullopt;

        const Quantity sum_base = total_executed_base(orders);
        if (sum_base == 0)
            return std::nullopt;

        return static_cast<double>(sum_quote) / sum_base;
    }

    std::optional<double> median_order_notional(std::span<const OrderRecord> orders)
    {
        if (orders.empty())
            return std::nullopt;

        std::vector<Quantity> notionals;
        notionals.reserve(orders.size());
        std::ranges::transform(orders, std::back_inserter(notionals), &OrderRecord::executed_quote_qty);

        const std::size_t n = notionals.size();
        const auto mid = notionals.begin() + n / 2;
        std::ranges::nth_element(notionals, mid);

        if ((n % 2) != 0)
            return static_cast<double>(*mid);

        const Quantity mid2 = *mid;
        std::ranges::nth_element(notionals, notionals.begin() + (n / 2 - 1));
        const Quantity mid1 = notionals[n / 2 - 1];

        return (static_cast<double>(mid1) + static_cast<double>(mid2)) / 2.0;
    }

    std::vector<std::pair<OrderId, Quantity>> top_n_by_executed_quote(std::span<const OrderRecord> orders, std::size_t n)
    {
        n = std::min(n, orders.size());
        if (n == 0)
            return {};

        std::vector<std::pair<OrderId, Quantity>> pairs;
        pairs.reserve(orders.size());

        std::ranges::transform(
            orders,
            std::back_inserter(pairs),
            [](const OrderRecord &order)
            {
                return std::pair{order.id, order.executed_quote_qty};
            });

        auto comparator = [](const auto &a, const auto &b)
        {
            if (a.second != b.second)
                return a.second > b.second;
            return a.first < b.first;
        };

        if (n < pairs.size())
        {
            const auto cut = pairs.begin() + n;
            std::ranges::nth_element(pairs, cut, comparator);
            pairs.resize(n);
        }

        std::ranges::sort(pairs, comparator);
        return pairs;
    }

    std::unordered_map<Market, Quantity> executed_quote_by_market(std::span<const OrderRecord> orders)
    {
        std::unordered_map<Market, Quantity> result{};
        result.reserve(orders.size());

        for (const OrderRecord &order : orders)
        {
            assert(!add_overflow_i64(result[order.market], order.executed_quote_qty));
            result[order.market] += order.executed_quote_qty;
        }

        return result;
    }

    std::optional<double> avg_slippage_bps_for_limits(std::span<const OrderRecord> orders)
    {
        if (orders.empty())
            return std::nullopt;

        auto is_valid_limit = [](const OrderRecord &order)
        {
            return order.type == OrderType::LimitOrder &&
                   order.avg_price.has_value() &&
                   order.limit_price.has_value() &&
                   *order.limit_price != 0;
        };

        auto to_slippage_bps = [](const OrderRecord &order)
        {
            if (order.side == Side::Sell)
                return ((*order.avg_price - *order.limit_price) / *order.limit_price) * 10000;
            return -1.0 * ((*order.avg_price - *order.limit_price) / *order.limit_price) * 10000;
        };

        double sum = 0.0;
        std::size_t count = 0;
        for (const OrderRecord &order : orders)
        {
            if (!is_valid_limit(order))
                continue;
            sum += to_slippage_bps(order);
            ++count;
        }

        if (count == 0)
            return std::nullopt;

        return sum / count;
    }

    std::vector<std::pair<Market, Quantity>> rank_markets_by_volume(std::span<const OrderRecord> orders)
    {
        auto markets_quantity_map = executed_quote_by_market(orders);

        std::vector<std::pair<Market, Quantity>> ranked_markets;
        ranked_markets.reserve(markets_quantity_map.size());

        for (const auto &entry : markets_quantity_map)
        {
            ranked_markets.emplace_back(entry.first, entry.second);
        }

        std::ranges::sort(
            ranked_markets,
            [](const auto &a, const auto &b)
            {
                return a.second > b.second;
            });

        return ranked_markets;
    }
} // namespace vertex::application
