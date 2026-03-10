#include "vertex/application/exchange.hpp"
#include "vertex/application/order_analytics.hpp"

#include <span>

namespace vertex::application
{
    std::expected<std::vector<OrderRecord>, AnalyticsError> Exchange::user_orders_snapshot(UserId user_id) const
    {
        if (!user_id.is_valid())
            return std::unexpected(AnalyticsError::InvalidUserId);

        if(!user_exists(user_id))
            return std::unexpected(AnalyticsError::UserNotFound);
        
        auto user_order_history = order_history_.find_by_user(user_id);

        if (!user_order_history || user_order_history.value().empty())
            return std::unexpected(AnalyticsError::NoData);

        return user_order_history.value();
    }

    std::expected<std::size_t, AnalyticsError> Exchange::order_count_by_status(UserId user_id, OrderStatus status) const
    {
        auto user_orders_history = user_orders_snapshot(user_id);
        if (!user_orders_history)
            return std::unexpected(user_orders_history.error());

        return vertex::application::analytics::count_by_status(std::span{user_orders_history.value()}, status);
    }

    std::expected<std::size_t, AnalyticsError> Exchange::order_count_by_side(UserId user_id, Side side) const
    {
        auto user_orders_history = user_orders_snapshot(user_id);
        if (!user_orders_history)
            return std::unexpected(user_orders_history.error());

        return vertex::application::analytics::count_by_side(std::span{user_orders_history.value()}, side);
    }

    std::expected<Quantity, AnalyticsError> Exchange::total_executed_base_by_user(UserId user_id) const
    {
        auto user_orders_history = user_orders_snapshot(user_id);
        if (!user_orders_history)
            return std::unexpected(user_orders_history.error());

        return vertex::application::analytics::total_executed_base(std::span{user_orders_history.value()});
    }

    std::expected<Quantity, AnalyticsError> Exchange::total_executed_quote_by_user(UserId user_id) const
    {
        auto user_orders_history = user_orders_snapshot(user_id);
        if (!user_orders_history)
            return std::unexpected(user_orders_history.error());

        return vertex::application::analytics::total_executed_quote(std::span{user_orders_history.value()});
    }

    std::expected<double, AnalyticsError> Exchange::average_fill_count_by_user(UserId user_id) const
    {
        auto user_orders_history = user_orders_snapshot(user_id);
        if (!user_orders_history)
            return std::unexpected(user_orders_history.error());

        auto result = vertex::application::analytics::average_fill_count(std::span{user_orders_history.value()});
        if (!result)
            return std::unexpected(AnalyticsError::NoData);

        return result.value();
    }

    std::expected<double, AnalyticsError> Exchange::completion_ratio_by_user(UserId user_id) const
    {
        auto user_orders_history = user_orders_snapshot(user_id);
        if (!user_orders_history)
            return std::unexpected(user_orders_history.error());

        auto result = vertex::application::analytics::completion_ratio(std::span{user_orders_history.value()});
        if (!result)
            return std::unexpected(AnalyticsError::NoData);

        return result.value();
    }

    std::expected<double, AnalyticsError> Exchange::avg_order_notional_by_user(UserId user_id) const
    {
        auto user_orders_history = user_orders_snapshot(user_id);
        if (!user_orders_history)
            return std::unexpected(user_orders_history.error());

        auto result = vertex::application::analytics::avg_order_notional(std::span{user_orders_history.value()});
        if (!result)
            return std::unexpected(AnalyticsError::NoData);

        return result.value();
    }

    std::expected<double, AnalyticsError> Exchange::vwap_from_orders_by_user(UserId user_id) const
    {
        auto user_orders_history = user_orders_snapshot(user_id);
        if (!user_orders_history)
            return std::unexpected(user_orders_history.error());

        auto result = vertex::application::analytics::vwap_from_orders(std::span{user_orders_history.value()});
        if (!result)
            return std::unexpected(AnalyticsError::NoData);

        return result.value();
    }

    std::expected<double, AnalyticsError> Exchange::median_order_notional_by_user(UserId user_id) const
    {
        auto user_orders_history = user_orders_snapshot(user_id);
        if (!user_orders_history)
            return std::unexpected(user_orders_history.error());

        auto result = vertex::application::analytics::median_order_notional(std::span{user_orders_history.value()});
        if (!result)
            return std::unexpected(AnalyticsError::NoData);

        return result.value();
    }

    std::expected<std::vector<std::pair<OrderId, Quantity>>, AnalyticsError>
    Exchange::top_n_by_executed_quote_by_user(UserId user_id, std::size_t n) const
    {
        auto user_orders_history = user_orders_snapshot(user_id);
        if (!user_orders_history)
            return std::unexpected(user_orders_history.error());

        return vertex::application::analytics::top_n_by_executed_quote(std::span{user_orders_history.value()}, n);
    }

    std::expected<std::unordered_map<Market, Quantity>, AnalyticsError>
    Exchange::executed_quote_by_market_for_user(UserId user_id) const
    {
        auto user_orders_history = user_orders_snapshot(user_id);
        if (!user_orders_history)
            return std::unexpected(user_orders_history.error());

        return vertex::application::analytics::executed_quote_by_market(std::span{user_orders_history.value()});
    }

    std::expected<double, AnalyticsError> Exchange::avg_slippage_bps_for_limits_by_user(UserId user_id) const
    {
        auto user_orders_history = user_orders_snapshot(user_id);
        if (!user_orders_history)
            return std::unexpected(user_orders_history.error());

        auto result = vertex::application::analytics::avg_slippage_bps_for_limits(std::span{user_orders_history.value()});
        if (!result)
            return std::unexpected(AnalyticsError::NoData);

        return result.value();
    }

    std::expected<std::vector<std::pair<Market, Quantity>>, AnalyticsError>
    Exchange::rank_markets_by_volume_for_user(UserId user_id) const
    {
        auto user_orders_history = user_orders_snapshot(user_id);
        if (!user_orders_history)
            return std::unexpected(user_orders_history.error());

        return vertex::application::analytics::rank_markets_by_volume(std::span{user_orders_history.value()});
    }
} // namespace vertex::application
