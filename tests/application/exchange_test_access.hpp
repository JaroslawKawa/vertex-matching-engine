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
    };
} // namespace vertex::application
