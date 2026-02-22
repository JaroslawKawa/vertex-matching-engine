#pragma once

#include <cstdint>
#include <string>
#include "vertex/core/strong_id.hpp"

namespace vertex::core{

    struct UserTag{};
    struct OrderTag{};
    

    using UserId = StrongId<UserTag>;
    using OrderId = StrongId<OrderTag>;

    using Price = std::int64_t; //price in small currency unit
    using Quantity = std::int64_t; //quantity in minimal lot unit

    using Symbol = std::string;

    enum class Side {
        Buy,
        Sell
    };
}// namespace vertex::core