#pragma once

#include "tests/application/order_meta_store_test_access.hpp"
#include "vertex/application/exchange.hpp"

namespace vertex::application
{
    class ExchangeTestAccess
    {
    public:
        static OrderMetaStoreTestAccess::Snapshot order_meta_snapshot(const Exchange &exchange)
        {
            return OrderMetaStoreTestAccess::snapshot(exchange.order_meta_store_);
        }

        static std::optional<OrderRecord> order_history_find(const Exchange &exchange, OrderId id)
        {
            return exchange.order_history_.find(id);
        }

        static std::optional<std::vector<OrderRecord>> order_history_find_by_user(const Exchange &exchange, UserId user_id)
        {
            return exchange.order_history_.find_by_user(user_id);
        }
    };
} // namespace vertex::application
