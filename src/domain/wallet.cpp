#include "vertex/domain/wallet.hpp"

namespace vertex::domain
{
    std::expected<void, WalletError> Wallet::deposit(const Symbol &symbol, const Quantity amount)
    {
        // Invalid amount
        if (amount <= 0)
            return std::unexpected(WalletError::InvalidAmount);

        auto it_symbol = balances_.find(symbol);

        // Symbol found in wallet balances
        if (it_symbol != balances_.end())
        {
            it_symbol->second.free += amount;
        }
        // Symbol not fount in wallet balances, create it
        else
        {

            balances_.emplace(symbol, Balance{amount, 0});
        }

        return {};
    }

    std::expected<void, WalletError> Wallet::withdraw(const Symbol &symbol, const Quantity amount)
    {
        // Invalid amount
        if (amount <= 0)
            return std::unexpected(WalletError::InvalidAmount);

        auto it_symbol = balances_.find(symbol);

        // Symbol not exists in wallet
        if (it_symbol == balances_.end())
            return std::unexpected(WalletError::InsufficientFunds);

        // Not enought free balance quantity of symbol to withdraw in wallet
        if (it_symbol->second.free < amount)
            return std::unexpected(WalletError::InsufficientFunds);

        it_symbol->second.free -= amount;

        return {};
    }

    std::expected<void, WalletError> Wallet::reserve(const Symbol &symbol, const Quantity amount)
    {
        // Invalid amount
        if (amount <= 0)
            return std::unexpected(WalletError::InvalidAmount);

        auto it_symbol = balances_.find(symbol);

        // Symbol not exists in wallet
        if (it_symbol == balances_.end())
            return std::unexpected(WalletError::InsufficientFunds);

        // Not enought free balance of symbol to reserve in wallet
        if (it_symbol->second.free < amount)
            return std::unexpected(WalletError::InsufficientFunds);

        it_symbol->second.free -= amount;
        it_symbol->second.reserved += amount;

        return {};
    }

    std::expected<void, WalletError> Wallet::release(const Symbol &symbol, const Quantity amount)
    {
        // Invalid amount
        if (amount <= 0)
            return std::unexpected(WalletError::InvalidAmount);

        auto it_symbol = balances_.find(symbol);

        // Symbol not exists in wallet
        if (it_symbol == balances_.end())
            return std::unexpected(WalletError::InsufficientReserved);

        // Not enought reserve balance of symbol to release in wallet
        if (it_symbol->second.reserved < amount)
            return std::unexpected(WalletError::InsufficientReserved);

        it_symbol->second.reserved -= amount;
        it_symbol->second.free += amount;

        return {};
    }

    Quantity Wallet::free_balance(const Symbol &symbol) const
    {

        auto it_symbol = balances_.find(symbol);

        //symbol not exist in wallet balance return 0;
        if (it_symbol == balances_.end())
            return 0;

        return it_symbol->second.free;
    }

    Quantity Wallet::reserved_balance(const Symbol &symbol) const
    {
        auto it_symbol = balances_.find(symbol);

        //symbol not exist in wallet balance return 0;
        if(it_symbol == balances_.end())
        return 0;

        return it_symbol->second.reserved;
    }

} // namespace vertex::domain
