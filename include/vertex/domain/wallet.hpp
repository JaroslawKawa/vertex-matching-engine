#pragma once

#include <unordered_map>
#include <expected>
#include "vertex/core/types.hpp"

namespace vertex::domain
{
    using Quantity = vertex::core::Quantity;
    using Symbol = vertex::core::Symbol;
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
        std::unordered_map<Symbol, Balance> balances_{};

    public:
        Wallet() = default;
        std::expected<void, WalletError> deposit(const Symbol &symbol, const Quantity amount);
        std::expected<void, WalletError> withdraw(const Symbol &symbol, const Quantity amount);
        std::expected<void, WalletError> reserve(const Symbol &symbol, const Quantity amount);
        std::expected<void, WalletError> release(const Symbol &symbol, const Quantity amount);

        Quantity free_balance(const Symbol &symbol) const;
        Quantity reserved_balance(const Symbol &symbol) const;
    };
} // namespace vertex::domain
