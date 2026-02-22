#include <cassert>
#include <stdexcept>
#include <vertex/application/exchange.hpp>

namespace vertex::application
{

    std::expected<UserId, ExchangeError> Exchange::create_user(std::string name)
    {

        if (name.empty())
            return std::unexpected(ExchangeError::EmptyName);

        User user{user_id_generator_.next(), name};

        auto [_, user_insert_result] = users_.emplace(user.id(), user);

        // User alread exists
        if (!user_insert_result)
            return std::unexpected(ExchangeError::UserAlreadyExists);

        auto [_, wallet_insert_result] = wallets_.emplace(user.id(), Wallet{});

        // Wallet with this user_id alread exists
        if (!wallet_insert_result)
        {
            assert(wallet_insert_result && "Invariant violated: wallet for new user id already exists");
            std::terminate();
        }

        return user.id();
    }

    std::expected<std::string, ExchangeError> Exchange::get_user_name(const UserId user_id) const
    {

        auto it_user = users_.find(user_id);

        if (it_user == users_.end())
            return std::unexpected(ExchangeError::UserNotFound);

        return it_user->second.name();
    }

    bool Exchange::user_exists(const UserId user_id) const
    {

        return users_.find(user_id) != users_.end();
    }

    std::expected<void, ExchangeError> Exchange::deposit(const UserId user_id, const Symbol &symbol, const Quantity quantity)
    {

        auto it_user_wallets = wallets_.find(user_id);

        // User not found
        if (it_user_wallets == wallets_.end())
            return std::unexpected(ExchangeError::UserNotFound);

        const auto result = it_user_wallets->second.deposit(symbol, quantity);

        // Wallet depsoit error
        if (!result)
        {
            auto wallet_error = result.error();
            switch (wallet_error)

            {
            case vertex::domain::WalletError::InvalidAmount:
                return std::unexpected(ExchangeError::InvalidAmount);

            default:
                assert(false && "Unexpected WalletError in deposit");
                std::terminate();
            }
        }

        return {};
    }

    std::expected<void, ExchangeError> Exchange::withdraw(const UserId user_id, const Symbol &symbol, const Quantity quantity)
    {
        auto it_user_wallets = wallets_.find(user_id);

        // User not found
        if (it_user_wallets == wallets_.end())
            return std::unexpected(ExchangeError::UserNotFound);

        const auto result = it_user_wallets->second.withdraw(symbol, quantity);

        if (!result)
        {
            auto wallet_error = result.error();
            switch (wallet_error)
            {
            case vertex::domain::WalletError::InsufficientFunds:
                return std::unexpected(ExchangeError::InsufficientFunds);

            case vertex::domain::WalletError::InvalidAmount:
                return std::unexpected(ExchangeError::InvalidAmount);

            default:
                assert(false && "Unexpected WalletError in withdraw");
                std::terminate();
            }
        }
        return {};
    }
    std::expected<void, ExchangeError> Exchange::reserve(const UserId user_id, const Symbol &symbol, const Quantity quantity)
    {
        auto it_user_wallets = wallets_.find(user_id);

        // User not found
        if (it_user_wallets == wallets_.end())
            return std::unexpected(ExchangeError::UserNotFound);

        const auto result = it_user_wallets->second.reserve(symbol, quantity);

        if (!result)
        {
            auto wallet_error = result.error();
            switch (wallet_error)
            {
            case vertex::domain::WalletError::InsufficientFunds:
                return std::unexpected(ExchangeError::InsufficientFunds);

            case vertex::domain::WalletError::InvalidAmount:
                return std::unexpected(ExchangeError::InvalidAmount);

            default:
                assert(false && "Unexpected WalletError in reserve");
                std::terminate();
            }
        }
        return {};
    }

    std::expected<void, ExchangeError> Exchange::release(const UserId user_id, const Symbol &symbol, const Quantity quantity)
    {
        auto it_user_wallets = wallets_.find(user_id);

        // User not found
        if (it_user_wallets == wallets_.end())
            return std::unexpected(ExchangeError::UserNotFound);

        const auto result = it_user_wallets->second.release(symbol, quantity);

        if (!result)
        {
            auto wallet_error = result.error();
            switch (wallet_error)
            {
            case vertex::domain::WalletError::InsufficientReserved:
                return std::unexpected(ExchangeError::InsufficientReserved);

            case vertex::domain::WalletError::InvalidAmount:
                return std::unexpected(ExchangeError::InvalidAmount);

            default:
                assert(false && "Unexpected WalletError in release");
                std::terminate();
            }
        }
        return {};
    }

    std::expected<Quantity, ExchangeError> Exchange::free_balance(const UserId user_id, const Symbol &symbol) const
    {

        auto it_user_wallets = wallets_.find(user_id);

        if (it_user_wallets == wallets_.end())
            return std::unexpected(ExchangeError::UserNotFound);

        return it_user_wallets->second.free_balance(symbol);
    }

    std::expected<Quantity, ExchangeError> Exchange::reserved_balance(const UserId user_id, const Symbol &symbol) const
    {
        auto it_user_wallets = wallets_.find(user_id);

        if (it_user_wallets == wallets_.end())
            return std::unexpected(ExchangeError::UserNotFound);

        return it_user_wallets->second.reserved_balance(symbol);
    }

} // namespace vertex::application