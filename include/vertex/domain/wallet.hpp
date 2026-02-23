#pragma once

#include <unordered_map>
#include <expected>
#include "vertex/core/types.hpp"

namespace vertex::domain
{
    using Quantity = vertex::core::Quantity;
    using Asset = vertex::core::Asset;
    enum class WalletError
    {
        InsufficientFunds,
        InsufficientReserved,
        InvalidAmount
    };

    struct Balance
    {
        vertex::core::Quantity free{0};
        vertex::core::Quantity reserved{0};
    };

    class Wallet
    {
    private:
        std::unordered_map<Asset, Balance> balances_{};

    public:
        Wallet() = default;
        std::expected<void, WalletError> deposit(const Asset &asset, const Quantity amount);
        std::expected<void, WalletError> withdraw(const Asset &asset, const Quantity amount);
        std::expected<void, WalletError> reserve(const Asset &asset, const Quantity amount);
        std::expected<void, WalletError> release(const Asset &asset, const Quantity amount);

        Quantity free_balance(const Asset &asset) const;
        Quantity reserved_balance(const Asset &asset) const;
    };
} // namespace vertex::domain
