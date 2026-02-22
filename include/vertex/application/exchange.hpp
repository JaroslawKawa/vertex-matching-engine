#pragma once

#include <unordered_map>
#include <expected>
#include <string>
#include "vertex/domain/user.hpp"
#include "vertex/domain/wallet.hpp"
#include "vertex/core/id_generator.hpp"
#include "vertex/core/types.hpp"
namespace vertex::application
{
    using UserId = vertex::core::UserId;
    using UserIdGenerator = vertex::core::IdGenerator<UserId>;
    using User = vertex::domain::User;
    using Wallet = vertex::domain::Wallet;
    using Quantity = vertex::core::Quantity;
    using Symbol = vertex::core::Symbol;

    enum class ExchangeError
    {
        UserNotFound,
        UserAlreadyExists,
        EmptyName,
        InsufficientFunds,
        InsufficientReserved,
        InvalidAmount

    };

    class Exchange
    {
    private:
        std::unordered_map<UserId, User> users_;
        std::unordered_map<UserId, Wallet> wallets_;
        UserIdGenerator user_id_generator_;

    public:
        Exchange() = default;
        std::expected<UserId, ExchangeError> create_user(std::string name);
        std::expected<std::string, ExchangeError> get_user_name(const UserId user_id) const;
        bool user_exists(const UserId user_id) const;

        std::expected<void, ExchangeError> deposit(const UserId user_id, const Symbol &symbol, const Quantity quantity);
        std::expected<void, ExchangeError> withdraw(const UserId user_id, const Symbol &symbol, const Quantity quantity);
        std::expected<void, ExchangeError> reserve(const UserId user_id, const Symbol &symbol, const Quantity quantity);
        std::expected<void, ExchangeError> release(const UserId user_id, const Symbol &symbol, const Quantity quantity);
        std::expected<Quantity, ExchangeError> free_balance(const UserId user_id, const Symbol &symbol) const;
        std::expected<Quantity, ExchangeError> reserved_balance(const UserId user_id, const Symbol &symbol) const;
    };
} // namespace vertex::application
