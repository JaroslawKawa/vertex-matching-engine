#pragma once
#include <variant>
#include "vertex/core/types.hpp"

namespace vertex::engine
{
    using Market = vertex::core::Market;
    using OrderId = vertex::core::OrderId;
    using UserId = vertex::core::UserId;
    using Price = vertex::core::Price;
    using Side = vertex::core::Side;
    using Quantity = vertex::core::Quantity;

    struct LimitOrderRequest
    {
        OrderId id;
        UserId user_id;
        Market market;
        Side side;
        Price limit_price;
        Quantity base_quantity;
    };

    struct MarketBuyByQuoteRequest
    {
        OrderId id;
        UserId user_id;
        Market market;
        Quantity quote_budget;
    };
    struct MarketSellByBaseRequest
    {
        OrderId id;
        UserId user_id;
        Market market;
        Quantity base_quantity;
    };

    using OrderRequest = std::variant<
        LimitOrderRequest,
        MarketBuyByQuoteRequest,
        MarketSellByBaseRequest>;

}