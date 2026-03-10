#pragma once

#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include "vertex/application/order_history.hpp"

namespace vertex::application::analytics
{
    std::size_t count_by_status(std::span<const OrderRecord> orders, OrderStatus status);
    std::size_t count_by_side(std::span<const OrderRecord> orders, Side side);
    Quantity total_executed_base(std::span<const OrderRecord> orders);
    Quantity total_executed_quote(std::span<const OrderRecord> orders);
    std::optional<double> average_fill_count(std::span<const OrderRecord> orders);
    std::optional<double> completion_ratio(std::span<const OrderRecord> orders);
    std::optional<double> avg_order_notional(std::span<const OrderRecord> orders);
    std::optional<double> vwap_from_orders(std::span<const OrderRecord> orders);
    std::optional<double> median_order_notional(std::span<const OrderRecord> orders);
    std::vector<std::pair<OrderId, Quantity>> top_n_by_executed_quote(std::span<const OrderRecord> orders, std::size_t n);
    std::unordered_map<Market, Quantity> executed_quote_by_market(std::span<const OrderRecord> orders);
    std::optional<double> avg_slippage_bps_for_limits(std::span<const OrderRecord> orders);
    std::vector<std::pair<Market, Quantity>> rank_markets_by_volume(std::span<const OrderRecord> orders);
} // namespace vertex::application::analytics
