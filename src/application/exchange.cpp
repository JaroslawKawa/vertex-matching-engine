#include <cassert>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vertex/application/exchange.hpp>
#include <vertex/engine/order_book.hpp>
namespace vertex::application
{
    using CancelResult = vertex::engine::CancelResult;

    namespace
    {
        PlaceOrderError map_to_place_order_error(EngineAsyncError error)
        {
            switch (error)
            {
            case EngineAsyncError::WorkerStopped:
                return PlaceOrderError::WorkerStopped;
            case EngineAsyncError::MarketNotFound:
                return PlaceOrderError::MarketNotListed;
            default:
                assert(false && "Unexpected EngineAsyncError in place order mapping");
                return PlaceOrderError::WorkerStopped;
            }
        }

        CancelOrderError map_to_cancel_order_error(EngineAsyncError error)
        {
            switch (error)
            {
            case EngineAsyncError::WorkerStopped:
                return CancelOrderError::WorkerStopped;
            case EngineAsyncError::MarketNotFound:
                return CancelOrderError::MarketNotFound;
            default:
                assert(false && "Unexpected EngineAsyncError in cancel order mapping");
                return CancelOrderError::WorkerStopped;
            }
        }

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

        struct TwoAccountLocks
        {
            std::unique_lock<std::mutex> first;
            std::optional<std::unique_lock<std::mutex>> second;
        };

        TwoAccountLocks lock_two_accounts(UserId a_id, Account &a_account, UserId b_id, Account &b_account)
        {
            TwoAccountLocks locks;

            if (a_id == b_id)
            {
                locks.first = std::unique_lock(a_account.mu);
                return locks;
            }

            if (a_id < b_id)
            {
                locks.first = std::unique_lock(a_account.mu);
                locks.second = std::unique_lock(b_account.mu);
            }
            else
            {
                locks.first = std::unique_lock(b_account.mu);
                locks.second = std::unique_lock(a_account.mu);
            }
            return locks;
        }
    }

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
            auto [_, user_insert_result] = accounts_.emplace(user.id(), std::make_shared<Account>(user, Wallet{}));
            if (!user_insert_result)
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

    std::expected<void, WalletOperationError> Exchange::deposit(const UserId user_id, const Asset &asset, const Quantity quantity)
    {
        std::shared_ptr<Account> account;
        {
            std::shared_lock lock(accounts_mu_);
            auto account_it = accounts_.find(user_id);
            if (account_it == accounts_.end())
                return std::unexpected(WalletOperationError::UserNotFound);
            account = account_it->second;
        }

        std::expected<void, WalletError> result;
        {
            std::lock_guard lock(account->mu);
            result = account->wallet.deposit(asset, quantity);
        }

        if (!result)
        {
            auto wallet_error = result.error();
            switch (wallet_error)

            {
            case WalletError::InvalidAmount:
                return std::unexpected(WalletOperationError::InvalidQuantity);

            default:
                assert(false && "Unexpected WalletError in deposit");
                std::terminate();
            }
        }

        return {};
    }

    std::expected<void, WalletOperationError> Exchange::withdraw(const UserId user_id, const Asset &asset, const Quantity quantity)
    {
        std::shared_ptr<Account> account;
        {
            std::shared_lock lock(accounts_mu_);
            auto account_it = accounts_.find(user_id);
            if (account_it == accounts_.end())
                return std::unexpected(WalletOperationError::UserNotFound);
            account = account_it->second;
        }

        std::expected<void, WalletError> result;
        {
            std::lock_guard lock(account->mu);
            result = account->wallet.withdraw(asset, quantity);
        }

        if (!result)
        {
            auto wallet_error = result.error();
            switch (wallet_error)
            {
            case vertex::domain::WalletError::InsufficientFunds:
                return std::unexpected(WalletOperationError::InsufficientFunds);

            case vertex::domain::WalletError::InvalidAmount:
                return std::unexpected(WalletOperationError::InvalidQuantity);

            default:
                assert(false && "Unexpected WalletError in withdraw");
                std::terminate();
            }
        }
        return {};
    }

    std::expected<void, WalletOperationError> Exchange::reserve(const UserId user_id, const Asset &asset, const Quantity quantity)
    {
        std::shared_ptr<Account> account;
        {
            std::shared_lock lock(accounts_mu_);
            auto account_it = accounts_.find(user_id);
            if (account_it == accounts_.end())
                return std::unexpected(WalletOperationError::UserNotFound);
            account = account_it->second;
        }

        std::expected<void, WalletError> result;
        {
            std::lock_guard lock(account->mu);
            result = account->wallet.reserve(asset, quantity);
        }

        if (!result)
        {
            auto wallet_error = result.error();
            switch (wallet_error)
            {
            case vertex::domain::WalletError::InsufficientFunds:
                return std::unexpected(WalletOperationError::InsufficientFunds);

            case vertex::domain::WalletError::InvalidAmount:
                return std::unexpected(WalletOperationError::InvalidQuantity);

            default:
                assert(false && "Unexpected WalletError in reserve");
                std::terminate();
            }
        }
        return {};
    }

    std::expected<void, WalletOperationError> Exchange::release(const UserId user_id, const Asset &asset, const Quantity quantity)
    {
        std::shared_ptr<Account> account;
        {
            std::shared_lock lock(accounts_mu_);
            auto account_it = accounts_.find(user_id);
            if (account_it == accounts_.end())
                return std::unexpected(WalletOperationError::UserNotFound);
            account = account_it->second;
        }

        std::expected<void, WalletError> result;
        {
            std::lock_guard lock(account->mu);
            result = account->wallet.release(asset, quantity);
        }

        if (!result)
        {
            auto wallet_error = result.error();
            switch (wallet_error)
            {
            case vertex::domain::WalletError::InsufficientReserved:
                return std::unexpected(WalletOperationError::InsufficientReserved);

            case vertex::domain::WalletError::InvalidAmount:
                return std::unexpected(WalletOperationError::InvalidQuantity);

            default:
                assert(false && "Unexpected WalletError in release");
                std::terminate();
            }
        }
        return {};
    }

    std::expected<Quantity, WalletOperationError> Exchange::free_balance(const UserId user_id, const Asset &asset) const
    {
        std::shared_ptr<Account> account;
        {
            std::shared_lock lock(accounts_mu_);
            auto account_it = accounts_.find(user_id);
            if (account_it == accounts_.end())
                return std::unexpected(WalletOperationError::UserNotFound);
            account = account_it->second;
        }

        std::lock_guard lock(account->mu);
        return account->wallet.free_balance(asset);
    }

    std::expected<Quantity, WalletOperationError> Exchange::reserved_balance(const UserId user_id, const Asset &asset) const
    {
        std::shared_ptr<Account> account;
        {
            std::shared_lock lock(accounts_mu_);
            auto account_it = accounts_.find(user_id);
            if (account_it == accounts_.end())
                return std::unexpected(WalletOperationError::UserNotFound);
            account = account_it->second;
        }

        std::lock_guard lock(account->mu);
        return account->wallet.reserved_balance(asset);
    }

    std::expected<OrderPlacementResult, PlaceOrderError> Exchange::place_limit_order(const UserId user_id, const Market &market, Side side, Price price, const Quantity quantity)
    {

        if (!user_id.is_valid())
            return std::unexpected(PlaceOrderError::UserNotFound);

        if (!market_dispatcher_.has_market(market))
            return std::unexpected(PlaceOrderError::MarketNotListed);

        if (quantity <= 0)
            return std::unexpected(PlaceOrderError::InvalidQuantity);

        if (price <= 0)
            return std::unexpected(PlaceOrderError::InvalidAmount);

        OrderPlacementResult order_result;
        Asset asset_to_reserve = (side == Side::Buy) ? market.quote() : market.base();
        Quantity quantity_to_reserve = (side == Side::Buy) ? price * quantity : quantity;

        std::shared_ptr<Account> account;
        {
            std::shared_lock lock(accounts_mu_);
            auto account_it = accounts_.find(user_id);
            if (account_it == accounts_.end())
                return std::unexpected(PlaceOrderError::UserNotFound);
            account = account_it->second;
        }

        std::expected<void, WalletError> reserve_result;
        {
            std::lock_guard lock(account->mu);
            reserve_result = account->wallet.reserve(asset_to_reserve, quantity_to_reserve);
        }
        if (!reserve_result)
            return std::unexpected(PlaceOrderError::InsufficientFunds);

        OrderId order_id;
        {
            std::lock_guard lock(order_id_generator_mu_);
            order_id = order_id_generator_.next();
        }

        if (!order_meta_store_.try_insert(order_id, OrderMeta{.owner = user_id, .market = market, .side = side, .price = price, .requested_base_qty = quantity}))
        {
            std::expected<void, WalletError> rollback_release_result;
            {
                std::lock_guard lock(account->mu);
                rollback_release_result = account->wallet.release(asset_to_reserve, quantity_to_reserve);
            }
            assert(rollback_release_result && "Invariant violated: rollback release failed after limit submit error");

            return std::unexpected(PlaceOrderError::OrderIdCollision);
        }

        LimitOrderRequest limit_order_request{
            .id = order_id,
            .user_id = user_id,
            .market = market,
            .side = side,
            .limit_price = price,
            .base_quantity = quantity};

        order_result.order_id = limit_order_request.id;
        order_result.remaining_quantity = quantity;
        order_result.filled_quantity = 0;

        auto matching_result_expected = market_dispatcher_.submit(std::move(limit_order_request)).get();

        if (!matching_result_expected)
        {
            std::expected<void, WalletError> rollback_release_result;
            {
                std::lock_guard lock(account->mu);
                rollback_release_result = account->wallet.release(asset_to_reserve, quantity_to_reserve);
            }
            assert(rollback_release_result && "Invariant violated: rollback release failed after limit submit error");
            order_meta_store_.erase(order_id);

            return std::unexpected(map_to_place_order_error(matching_result_expected.error()));
        }

        std::vector<Execution> matching_result = matching_result_expected.value();

        if (!matching_result.empty())
        {

            for (const Execution &execution : matching_result)
            {

                OrderId buyer_order_id = execution.buy_order_id;
                OrderId seller_order_id = execution.sell_order_id;

                auto buyer_order_meta = order_meta_store_.find(buyer_order_id);
                assert(buyer_order_meta != std::nullopt);
                UserId buyer_user_id = buyer_order_meta->owner;

                auto seller_order_meta = order_meta_store_.find(seller_order_id);
                assert(seller_order_meta != std::nullopt);
                UserId seller_user_id = seller_order_meta->owner;

                std::shared_ptr<Account> buyer;
                std::shared_ptr<Account> seller;

                {
                    std::shared_lock lock(accounts_mu_);
                    buyer = accounts_.find(buyer_user_id)->second;
                    seller = accounts_.find(seller_user_id)->second;
                }
                {
                    auto locks = lock_two_accounts(buyer->user.id(), *buyer, seller->user.id(), *seller);
                    const auto buyer_consume_result = buyer->wallet.consume_reserved(market.quote(), execution.execution_price * execution.quantity);
                    assert(buyer_consume_result && "Invariant violated: buyer reserved quote must cover executed notional");

                    Quantity refund = execution.buy_order_limit_price * execution.quantity - execution.execution_price * execution.quantity;
                    if (0 < refund)
                    {
                        const auto buyer_release_result = buyer->wallet.release(market.quote(), refund);
                        assert(buyer_release_result && "Invariant violated: buyer refund release failed");
                    }
                    const auto buyer_deposit_result = buyer->wallet.deposit(market.base(), execution.quantity);
                    assert(buyer_deposit_result && "Invariant violated: buyer base deposit failed");
                    const auto seller_consume_result = seller->wallet.consume_reserved(market.base(), execution.quantity);
                    assert(seller_consume_result && "Invariant violated: seller reserved base must cover executed quantity");

                    const auto seller_deposit_result = seller->wallet.deposit(market.quote(), execution.execution_price * execution.quantity);
                    assert(seller_deposit_result && "Invariant violated: seller quote deposit failed");
                }
                order_result.remaining_quantity -= execution.quantity;
                order_result.filled_quantity += execution.quantity;

                TradeId trade_id;
                {
                    std::lock_guard lock(trade_id_generator_mu_);
                    trade_id = trade_id_generator_.next();
                }
                Trade trade{trade_id, buyer_user_id, seller_user_id, buyer_order_id, seller_order_id, market, execution.quantity, execution.execution_price};

                order_meta_store_.append_fill(execution.buy_order_id, trade_id, execution.quantity, execution.execution_price);
                order_meta_store_.append_fill(execution.sell_order_id, trade_id, execution.quantity, execution.execution_price);

                trade_history_.add(std::move(trade));

                if (execution.buy_fully_filled)
                {
                    order_meta_store_.erase(execution.buy_order_id);
                }
                if (execution.sell_fully_filled)
                {
                    order_meta_store_.erase(execution.sell_order_id);
                }
            }
        }

        return order_result;
    }

    std::expected<OrderPlacementResult, PlaceOrderError> Exchange::execute_market_order(const UserId user_id, const Market &market, const Side side, const Quantity order_quantity)
    {
        if (!user_id.is_valid())
            return std::unexpected(PlaceOrderError::UserNotFound);

        if (!market_dispatcher_.has_market(market))
            return std::unexpected(PlaceOrderError::MarketNotListed);

        if (order_quantity <= 0)
            return std::unexpected(PlaceOrderError::InvalidQuantity);

        Asset asset_to_reserve = (side == Side::Buy) ? market.quote() : market.base();

        std::shared_ptr<Account> account;
        {
            std::shared_lock lock(accounts_mu_);
            auto account_it = accounts_.find(user_id);
            if (account_it == accounts_.end())
                return std::unexpected(PlaceOrderError::UserNotFound);
            account = account_it->second;
        }
        std::expected<void, WalletError> reserve_result;
        {
            std::lock_guard lock(account->mu);
            reserve_result = account->wallet.reserve(asset_to_reserve, order_quantity);
        }

        if (!reserve_result)
            return std::unexpected(PlaceOrderError::InsufficientFunds);

        std::expected<OrderPlacementResult, PlaceOrderError> order_result;

        if (Side::Buy == side)
        {
            order_result = execute_market_buy_by_quote(user_id, market, order_quantity);
        }
        else
        {
            order_result = execute_market_sell_by_base(user_id, market, order_quantity);
        }

        if (!order_result)
        {
            std::lock_guard lock(account->mu);
            const auto rollback_release_result = account->wallet.release(asset_to_reserve, order_quantity);
            assert(rollback_release_result && "Invariant violated: rollback release failed after market submit error");
        }

        return order_result;
    }

    std::expected<OrderPlacementResult, PlaceOrderError> Exchange::execute_market_buy_by_quote(const UserId user_id, const Market &market, const Quantity order_quantity)
    {

        OrderPlacementResult order_result;

        OrderId order_id;
        {
            std::lock_guard lock(order_id_generator_mu_);
            order_id = order_id_generator_.next();
        }
        MarketBuyByQuoteRequest order_request{
            .id = order_id,
            .user_id = user_id,
            .market = market,
            .quote_budget = order_quantity

        };

        order_result.order_id = order_request.id;
        order_result.remaining_quantity = order_quantity;
        order_result.filled_quantity = 0;

        auto execution_result_expected = market_dispatcher_.submit(std::move(order_request)).get();

        if (!execution_result_expected)
        {
            return std::unexpected(map_to_place_order_error(execution_result_expected.error()));
        }

        std::vector<Execution> execution_result = execution_result_expected.value();

        std::shared_ptr<Account> buyer;
        {
            std::shared_lock lock(accounts_mu_);
            auto account_it = accounts_.find(user_id);
            if (account_it == accounts_.end())
                return std::unexpected(PlaceOrderError::UserNotFound);
            buyer = account_it->second;
        }

        for (const auto &execution : execution_result)
        {
            OrderId seller_order_id = execution.sell_order_id;
            auto seller_order_meta = order_meta_store_.find(seller_order_id);
            assert(seller_order_meta != std::nullopt);
            UserId seller_user_id = seller_order_meta->owner;

            std::shared_ptr<Account> seller;
            {
                std::shared_lock lock(accounts_mu_);
                auto account_it = accounts_.find(seller_user_id);
                if (account_it == accounts_.end())
                    return std::unexpected(PlaceOrderError::UserNotFound);
                seller = account_it->second;
            }
            {
                auto locks = lock_two_accounts(buyer->user.id(), *buyer, seller->user.id(), *seller);
                const auto buyer_consume_result = buyer->wallet.consume_reserved(market.quote(), execution.quantity * execution.execution_price);
                assert(buyer_consume_result && "Invariant violated: buyer reserved quote must cover executed notional");
                const auto buyer_deposit_result = buyer->wallet.deposit(market.base(), execution.quantity);
                assert(buyer_deposit_result && "Invariant violated: buyer base deposit failed");
                const auto seller_consume_result = seller->wallet.consume_reserved(market.base(), execution.quantity);
                assert(seller_consume_result && "Invariant violated: seller reserved base must cover executed quantity");
                const auto seller_deposit_result = seller->wallet.deposit(market.quote(), execution.quantity * execution.execution_price);
                assert(seller_deposit_result && "Invariant violated: seller quote deposit failed");
            }
            order_result.remaining_quantity -= execution.quantity * execution.execution_price;
            order_result.filled_quantity += execution.quantity * execution.execution_price;

            TradeId trade_id;
            {
                std::lock_guard lock(trade_id_generator_mu_);
                trade_id = trade_id_generator_.next();
            }
            Trade trade{trade_id, user_id, seller_user_id, order_result.order_id, seller_order_id, market, execution.quantity, execution.execution_price};

            trade_history_.add(std::move(trade));

            if (execution.sell_fully_filled)
            {
                order_meta_store_.erase(execution.sell_order_id);
            }
        }
        if (order_result.remaining_quantity > 0)
        {
            std::lock_guard lock(buyer->mu);
            const auto taker_release_result = buyer->wallet.release(market.quote(), order_result.remaining_quantity);
            assert(taker_release_result && "Invariant violated: taker release failed after market buy");
        }
        return order_result;
    }

    std::expected<OrderPlacementResult, PlaceOrderError> Exchange::execute_market_sell_by_base(const UserId user_id, const Market &market, const Quantity order_quantity)
    {
        OrderPlacementResult order_result;

        OrderId order_id;
        {
            std::lock_guard lock(order_id_generator_mu_);
            order_id = order_id_generator_.next();
        }

        MarketSellByBaseRequest order_request{
            .id = order_id,
            .user_id = user_id,
            .market = market,
            .base_quantity = order_quantity

        };

        order_result.order_id = order_request.id;
        order_result.remaining_quantity = order_quantity;
        order_result.filled_quantity = 0;

        auto execution_result_expected = market_dispatcher_.submit(std::move(order_request)).get();

        if (!execution_result_expected)
        {
            return std::unexpected(map_to_place_order_error(execution_result_expected.error()));
        }

        std::vector<Execution> execution_result = execution_result_expected.value();

        std::shared_ptr<Account> seller;
        {
            std::shared_lock lock(accounts_mu_);
            auto account_it = accounts_.find(user_id);
            if (account_it == accounts_.end())
                return std::unexpected(PlaceOrderError::UserNotFound);
            seller = account_it->second;
        }

        for (const auto &execution : execution_result)
        {
            OrderId buyer_order_id = execution.buy_order_id;

            auto buyer_order_meta = order_meta_store_.find(buyer_order_id);
            assert(buyer_order_meta != std::nullopt);
            UserId buyer_user_id = buyer_order_meta->owner;

            std::shared_ptr<Account> buyer;
            {
                std::shared_lock lock(accounts_mu_);
                auto account_it = accounts_.find(buyer_user_id);
                if (account_it == accounts_.end())
                    return std::unexpected(PlaceOrderError::UserNotFound);
                buyer = account_it->second;
            }

            {
                auto locks = lock_two_accounts(buyer->user.id(), *buyer, seller->user.id(), *seller);
                const auto buyer_consume_result = buyer->wallet.consume_reserved(market.quote(), execution.quantity * execution.execution_price);
                assert(buyer_consume_result && "Invariant violated: buyer reserved quote must cover executed notional");
                const auto buyer_deposit_result = buyer->wallet.deposit(market.base(), execution.quantity);
                assert(buyer_deposit_result && "Invariant violated: buyer base deposit failed");
                const auto seller_consume_result = seller->wallet.consume_reserved(market.base(), execution.quantity);
                assert(seller_consume_result && "Invariant violated: seller reserved base must cover executed quantity");
                const auto seller_deposit_result = seller->wallet.deposit(market.quote(), execution.quantity * execution.execution_price);
                assert(seller_deposit_result && "Invariant violated: seller quote deposit failed");
            }
            order_result.remaining_quantity -= execution.quantity;
            order_result.filled_quantity += execution.quantity;

            TradeId trade_id;
            {
                std::lock_guard lock(trade_id_generator_mu_);
                trade_id = trade_id_generator_.next();
            }
            Trade trade{trade_id, buyer_user_id, user_id, buyer_order_id, order_result.order_id, market, execution.quantity, execution.execution_price};

            trade_history_.add(std::move(trade));

            if (execution.buy_fully_filled)
            {
                order_meta_store_.erase(execution.buy_order_id);
            }
        }
        if (order_result.remaining_quantity > 0)
        {
            std::lock_guard lock(seller->mu);
            const auto taker_release_result = seller->wallet.release(market.base(), order_result.remaining_quantity);
            assert(taker_release_result && "Invariant violated: taker release failed after market sell");
        }

        return order_result;
    }

    std::expected<CancelOrderResult, CancelOrderError> Exchange::cancel_order(const UserId user_id, const OrderId order_id)
    {
        {
            std::shared_lock lock(accounts_mu_);
            if (accounts_.find(user_id) == accounts_.end())
                return std::unexpected(CancelOrderError::UserNotFound);
        }
        const auto order = order_meta_store_.find(order_id);

        if (order == std::nullopt)
            return std::unexpected(CancelOrderError::OrderNotFound);

        if (order->owner != user_id)
            return std::unexpected(CancelOrderError::NotOrderOwner);

        auto cancel_result_expected = market_dispatcher_.cancel(order->market, order_id).get();

        if (!cancel_result_expected)
        {
            return std::unexpected(map_to_cancel_order_error(cancel_result_expected.error()));
        }

        auto cancel_result = cancel_result_expected.value();

        if (cancel_result == std::nullopt)
        {
            return std::unexpected(CancelOrderError::OrderNotFound);
        }

        std::shared_ptr<Account> account;
        {
            std::shared_lock lock(accounts_mu_);
            auto account_it = accounts_.find(user_id);
            if (account_it == accounts_.end())
                return std::unexpected(CancelOrderError::UserNotFound);
            account = account_it->second;
        }
        CancelOrderResult result;
        if (cancel_result->side == Side::Buy)
        {
            {
                std::lock_guard lock(account->mu);
                const auto buyer_release_result = account->wallet.release(order->market.quote(), cancel_result->remaining_quantity * cancel_result->price);
                assert(buyer_release_result && "Invariant violated: buyer release failed");
            }
            result.side = Side::Buy;
        }
        else
        {
            std::lock_guard lock(account->mu);
            {
                const auto seller_release_result = account->wallet.release(order->market.base(), cancel_result->remaining_quantity);
                assert(seller_release_result && "Invariant violated: seller release failed");
            }
            result.side = Side::Sell;
        }

        result.id = order_id;
        result.remaining_quantity = cancel_result->remaining_quantity;
        order_meta_store_.erase(order_id);

        return result;
    }

    std::expected<void, RegisterMarketError> Exchange::register_market(const Market &market)
    {

        auto register_result = market_dispatcher_.register_market(market);

        if (!register_result)
        {
            return std::unexpected(map_to_register_market_error(register_result.error()));
        }

        return {};
    }
} // namespace vertex::application
