#include "vertex/application/exchange.hpp"

#include <cassert>

namespace vertex::application
{
    namespace
    {
        RegisterMarketError map_to_register_market_error(EngineAsyncError error)
        {
            switch (error)
            {
            case EngineAsyncError::WorkerStopped:
                return RegisterMarketError::WorkerStopped;
            case EngineAsyncError::MarketAlreadyRegistered:
                return RegisterMarketError::AlreadyListed;
            default:
                assert(false && "Unexpected EngineAsyncError in register market mapping");
                return RegisterMarketError::WorkerStopped;
            }
        }
    } // namespace

    std::expected<UserId, UserError> Exchange::create_user(std::string name)
    {
        if (name.empty())
            return std::unexpected(UserError::EmptyName);

        UserId user_id;
        {
            std::lock_guard lock(user_id_generator_mu_);
            user_id = user_id_generator_.next();
        }

        User user{user_id, name};

        {
            std::lock_guard lock(accounts_mu_);
            auto [_, inserted] = accounts_.emplace(user.id(), std::make_shared<Account>(user, Wallet{}));
            if (!inserted)
                return std::unexpected(UserError::UserAlreadyExists);
        }

        return user.id();
    }

    std::expected<std::string, UserError> Exchange::get_user_name(const UserId user_id) const
    {
        std::shared_lock lock(accounts_mu_);
        auto it_user = accounts_.find(user_id);

        if (it_user == accounts_.end())
            return std::unexpected(UserError::UserNotFound);

        return it_user->second->user.name();
    }

    bool Exchange::user_exists(const UserId user_id) const
    {
        std::shared_lock lock(accounts_mu_);
        return accounts_.find(user_id) != accounts_.end();
    }

    std::expected<void, RegisterMarketError> Exchange::register_market(const Market &market)
    {
        auto register_result = market_dispatcher_.register_market(market);
        if (!register_result)
            return std::unexpected(map_to_register_market_error(register_result.error()));

        return {};
    }

} // namespace vertex::application
