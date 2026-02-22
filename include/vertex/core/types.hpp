#pragma once

#include <cstdint>
#include <string>

namespace vertex::core{
    using UserId = std::uint64_t;
    using OrderId = std::uint64_t;

    using Price = std::int64_t; //price in small currency unit
    using Quantity = std::int64_t; //quantity in minimal lot unit

    using Symbol = std::string;

    enum class Side {
        Buy,
        Sell
    };
}