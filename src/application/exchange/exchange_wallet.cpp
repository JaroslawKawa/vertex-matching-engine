#include "vertex/application/exchange.hpp"

#include <cassert>
#include <exception>

namespace vertex::application
{
    namespace
    {
        WalletOperationError map_to_wallet_error(vertex::domain::WalletError error)
        {
            switch (error)
            {
            case vertex::domain::WalletError::InsufficientReserved:
                return WalletOperationError::InsufficientReserved;
            case vertex::domain::WalletError::InsufficientFunds:
                return WalletOperationError::InsufficientFunds;
            case vertex::domain::WalletError::InvalidAmount:
                return WalletOperationError::InvalidQuantity;
            default:
                assert(false && "Unexpected WalletError in release");
                std::terminate();
            }
        }
    } // namespace

    std::expected<void, WalletOperationError> Exchange::deposit(
        const UserId user_id,
        const Asset &asset,
        const Quantity quantity)
    {
        std::shared_ptr<Account> account = get_account(user_id);
        if (account == nullptr)
            return std::unexpected(WalletOperationError::UserNotFound);

        std::expected<void, WalletError> result;
        {
            std::lock_guard lock(account->mu);
            result = account->wallet.deposit(asset, quantity);
        }

        if (!result)
            return std::unexpected(map_to_wallet_error(result.error()));

        return {};
    }

    std::expected<void, WalletOperationError> Exchange::withdraw(
        const UserId user_id,
        const Asset &asset,
        const Quantity quantity)
    {
        std::shared_ptr<Account> account = get_account(user_id);
        if (account == nullptr)
            return std::unexpected(WalletOperationError::UserNotFound);

        std::expected<void, WalletError> result;
        {
            std::lock_guard lock(account->mu);
            result = account->wallet.withdraw(asset, quantity);
        }

        if (!result)
            return std::unexpected(map_to_wallet_error(result.error()));

        return {};
    }

    std::expected<void, WalletOperationError> Exchange::reserve(
        const UserId user_id,
        const Asset &asset,
        const Quantity quantity)
    {
        std::shared_ptr<Account> account = get_account(user_id);
        if (account == nullptr)
            return std::unexpected(WalletOperationError::UserNotFound);

        std::expected<void, WalletError> result;
        {
            std::lock_guard lock(account->mu);
            result = account->wallet.reserve(asset, quantity);
        }

        if (!result)
            return std::unexpected(map_to_wallet_error(result.error()));

        return {};
    }

    std::expected<void, WalletOperationError> Exchange::release(
        const UserId user_id,
        const Asset &asset,
        const Quantity quantity)
    {
        std::shared_ptr<Account> account = get_account(user_id);
        if (account == nullptr)
            return std::unexpected(WalletOperationError::UserNotFound);

        std::expected<void, WalletError> result;
        {
            std::lock_guard lock(account->mu);
            result = account->wallet.release(asset, quantity);
        }

        if (!result)
            return std::unexpected(map_to_wallet_error(result.error()));

        return {};
    }

    std::expected<Quantity, WalletOperationError> Exchange::free_balance(const UserId user_id, const Asset &asset) const
    {
        std::shared_ptr<Account> account = get_account(user_id);
        if (account == nullptr)
            return std::unexpected(WalletOperationError::UserNotFound);

        std::lock_guard lock(account->mu);
        return account->wallet.free_balance(asset);
    }

    std::expected<Quantity, WalletOperationError> Exchange::reserved_balance(const UserId user_id, const Asset &asset) const
    {
        std::shared_ptr<Account> account = get_account(user_id);
        if (account == nullptr)
            return std::unexpected(WalletOperationError::UserNotFound);

        std::lock_guard lock(account->mu);
        return account->wallet.reserved_balance(asset);
    }

} // namespace vertex::application
