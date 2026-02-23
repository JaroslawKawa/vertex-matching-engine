#include "vertex/domain/wallet.hpp"

namespace vertex::domain
{
    std::expected<void, WalletError> Wallet::deposit(const Asset &asset, const Quantity amount)
    {
        // Invalid amount
        if (amount <= 0)
            return std::unexpected(WalletError::InvalidAmount);

        auto it_asset = balances_.find(asset);

        // Asset found in wallet balances
        if (it_asset != balances_.end())
        {
            it_asset->second.free += amount;
        }
        // Asset not fount in wallet balances, create it
        else
        {

            balances_.emplace(asset, Balance{amount, 0});
        }

        return {};
    }

    std::expected<void, WalletError> Wallet::withdraw(const Asset &asset, const Quantity amount)
    {
        // Invalid amount
        if (amount <= 0)
            return std::unexpected(WalletError::InvalidAmount);

        auto it_asset = balances_.find(asset);

        // Asset not exists in wallet
        if (it_asset == balances_.end())
            return std::unexpected(WalletError::InsufficientFunds);

        // Not enought free balance quantity of asset to withdraw in wallet
        if (it_asset->second.free < amount)
            return std::unexpected(WalletError::InsufficientFunds);

        it_asset->second.free -= amount;

        return {};
    }

    std::expected<void, WalletError> Wallet::reserve(const Asset &asset, const Quantity amount)
    {
        // Invalid amount
        if (amount <= 0)
            return std::unexpected(WalletError::InvalidAmount);

        auto it_asset = balances_.find(asset);

        // Asset not exists in wallet
        if (it_asset == balances_.end())
            return std::unexpected(WalletError::InsufficientFunds);

        // Not enought free balance of asset to reserve in wallet
        if (it_asset->second.free < amount)
            return std::unexpected(WalletError::InsufficientFunds);

        it_asset->second.free -= amount;
        it_asset->second.reserved += amount;

        return {};
    }

    std::expected<void, WalletError> Wallet::release(const Asset &asset, const Quantity amount)
    {
        // Invalid amount
        if (amount <= 0)
            return std::unexpected(WalletError::InvalidAmount);

        auto it_asset = balances_.find(asset);

        // Asset not exists in wallet
        if (it_asset == balances_.end())
            return std::unexpected(WalletError::InsufficientReserved);

        // Not enought reserve balance of asset to release in wallet
        if (it_asset->second.reserved < amount)
            return std::unexpected(WalletError::InsufficientReserved);

        it_asset->second.reserved -= amount;
        it_asset->second.free += amount;

        return {};
    }

    Quantity Wallet::free_balance(const Asset &asset) const
    {

        auto it_asset = balances_.find(asset);

        //asset not exist in wallet balance return 0;
        if (it_asset == balances_.end())
            return 0;

        return it_asset->second.free;
    }

    Quantity Wallet::reserved_balance(const Asset &asset) const
    {
        auto it_asset = balances_.find(asset);

        //asset not exist in wallet balance return 0;
        if(it_asset == balances_.end())
        return 0;

        return it_asset->second.reserved;
    }

} // namespace vertex::domain
