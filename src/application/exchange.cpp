#include <cassert>
#include <memory>
#include <stdexcept>
#include <vertex/application/exchange.hpp>
#include <vertex/engine/order_book.hpp>
namespace vertex::application
{
    using CancelResult = vertex::engine::CancelResult;

    std::expected<UserId, UserError> Exchange::create_user(std::string name)
    {

        if (name.empty())
            return std::unexpected(UserError::EmptyName);

        User user{user_id_generator_.next(), name};

        auto [_, user_insert_result] = users_.emplace(user.id(), user);

        // User alread exists
        if (!user_insert_result)
            return std::unexpected(UserError::UserAlreadyExists);

        auto [__, wallet_insert_result] = wallets_.emplace(user.id(), Wallet{});

        // Wallet with this user_id alread exists
        if (!wallet_insert_result)
        {
            assert(wallet_insert_result && "Invariant violated: wallet for new user id already exists");
            std::terminate();
        }

        return user.id();
    }

    std::expected<std::string, UserError> Exchange::get_user_name(const UserId user_id) const
    {

        auto it_user = users_.find(user_id);

        if (it_user == users_.end())
            return std::unexpected(UserError::UserNotFound);

        return it_user->second.name();
    }

    bool Exchange::user_exists(const UserId user_id) const
    {

        return users_.find(user_id) != users_.end();
    }

    std::expected<void, WalletOperationError> Exchange::deposit(const UserId user_id, const Asset &asset, const Quantity quantity)
    {

        auto it_user_wallets = wallets_.find(user_id);

        // User not found
        if (it_user_wallets == wallets_.end())
            return std::unexpected(WalletOperationError::UserNotFound);

        const auto result = it_user_wallets->second.deposit(asset, quantity);

        // Wallet depsoit error
        if (!result)
        {
            auto wallet_error = result.error();
            switch (wallet_error)

            {
            case vertex::domain::WalletError::InvalidAmount:
                return std::unexpected(WalletOperationError::InvalidAmount);

            default:
                assert(false && "Unexpected WalletError in deposit");
                std::terminate();
            }
        }

        return {};
    }

    std::expected<void, WalletOperationError> Exchange::withdraw(const UserId user_id, const Asset &asset, const Quantity quantity)
    {
        auto it_user_wallets = wallets_.find(user_id);

        // User not found
        if (it_user_wallets == wallets_.end())
            return std::unexpected(WalletOperationError::UserNotFound);

        const auto result = it_user_wallets->second.withdraw(asset, quantity);

        if (!result)
        {
            auto wallet_error = result.error();
            switch (wallet_error)
            {
            case vertex::domain::WalletError::InsufficientFunds:
                return std::unexpected(WalletOperationError::InsufficientFunds);

            case vertex::domain::WalletError::InvalidAmount:
                return std::unexpected(WalletOperationError::InvalidAmount);

            default:
                assert(false && "Unexpected WalletError in withdraw");
                std::terminate();
            }
        }
        return {};
    }

    std::expected<void, WalletOperationError> Exchange::reserve(const UserId user_id, const Asset &asset, const Quantity quantity)
    {
        auto it_user_wallets = wallets_.find(user_id);

        // User not found
        if (it_user_wallets == wallets_.end())
            return std::unexpected(WalletOperationError::UserNotFound);

        const auto result = it_user_wallets->second.reserve(asset, quantity);

        if (!result)
        {
            auto wallet_error = result.error();
            switch (wallet_error)
            {
            case vertex::domain::WalletError::InsufficientFunds:
                return std::unexpected(WalletOperationError::InsufficientFunds);

            case vertex::domain::WalletError::InvalidAmount:
                return std::unexpected(WalletOperationError::InvalidAmount);

            default:
                assert(false && "Unexpected WalletError in reserve");
                std::terminate();
            }
        }
        return {};
    }

    std::expected<void, WalletOperationError> Exchange::release(const UserId user_id, const Asset &asset, const Quantity quantity)
    {
        auto it_user_wallets = wallets_.find(user_id);

        // User not found
        if (it_user_wallets == wallets_.end())
            return std::unexpected(WalletOperationError::UserNotFound);

        const auto result = it_user_wallets->second.release(asset, quantity);

        if (!result)
        {
            auto wallet_error = result.error();
            switch (wallet_error)
            {
            case vertex::domain::WalletError::InsufficientReserved:
                return std::unexpected(WalletOperationError::InsufficientReserved);

            case vertex::domain::WalletError::InvalidAmount:
                return std::unexpected(WalletOperationError::InvalidAmount);

            default:
                assert(false && "Unexpected WalletError in release");
                std::terminate();
            }
        }
        return {};
    }

    std::expected<Quantity, WalletOperationError> Exchange::free_balance(const UserId user_id, const Asset &asset) const
    {

        auto it_user_wallets = wallets_.find(user_id);

        if (it_user_wallets == wallets_.end())
            return std::unexpected(WalletOperationError::UserNotFound);

        return it_user_wallets->second.free_balance(asset);
    }

    std::expected<Quantity, WalletOperationError> Exchange::reserved_balance(const UserId user_id, const Asset &asset) const
    {
        auto it_user_wallets = wallets_.find(user_id);

        if (it_user_wallets == wallets_.end())
            return std::unexpected(WalletOperationError::UserNotFound);

        return it_user_wallets->second.reserved_balance(asset);
    }

    std::expected<OrderPlacementResult, PlaceOrderError> Exchange::place_limit_order(const UserId user_id, const Market &market, Side side, Price price, const Quantity quantity)
    {

        if (!user_id.is_valid())
            return std::unexpected(PlaceOrderError::UserNotFound);

        if (!matching_engine_.has_market(market))
            return std::unexpected(PlaceOrderError::MarketNotListed);

        if (quantity <= 0)
            return std::unexpected(PlaceOrderError::InvalidQuantity);

        auto user_it = wallets_.find(user_id);

        if (user_it == wallets_.end())
            return std::unexpected(PlaceOrderError::UserNotFound);

        if (price <= 0)
            return std::unexpected(PlaceOrderError::InvalidAmount);

        OrderPlacementResult order_result;
        Asset asset_to_reserve = (side == Side::Buy) ? market.quote() : market.base();
        Quantity quantity_to_reserve = (side == Side::Buy) ? price * quantity : quantity;

        auto &wallet = wallets_.find(user_id)->second;
        auto reserve_result = wallet.reserve(asset_to_reserve, quantity_to_reserve);

        if (!reserve_result)
            return std::unexpected(PlaceOrderError::InsufficientFunds);

        std::unique_ptr<Order> order = std::make_unique<LimitOrder>(order_id_generator_.next(), user_id, market, side, quantity, price);

        orders_[order->id()] = user_id;
        orders_market_.insert_or_assign(order->id(), market);

        order_result.order_id = order->id();
        order_result.remaining_quantity = quantity;
        order_result.filled_quantity = 0;

        std::vector<Execution> matching_result = matching_engine_.add_order(std::move(order));

        if (!matching_result.empty())
        {

            for (const Execution &execution : matching_result)
            {

                OrderId buyer_order_id = execution.buy_order_id;
                OrderId seller_order_id = execution.sell_order_id;
                assert(orders_.find(buyer_order_id) != orders_.end());
                assert(orders_.find(seller_order_id) != orders_.end());
                UserId buyer_user_id = orders_.find(buyer_order_id)->second;
                UserId seller_user_id = orders_.find(seller_order_id)->second;

                Wallet &buyer_wallet = wallets_.find(buyer_user_id)->second;
                Wallet &seller_wallet = wallets_.find(seller_user_id)->second;

                const auto buyer_consume_result = buyer_wallet.consume_reserved(market.quote(), execution.execution_price * execution.quantity);
                assert(buyer_consume_result && "Invariant violated: buyer reserved quote must cover executed notional");

                Quantity refund = execution.buy_order_limit_price * execution.quantity - execution.execution_price * execution.quantity;
                if (0 < refund)
                {
                    const auto buyer_release_result = buyer_wallet.release(market.quote(), refund);
                    assert(buyer_release_result && "Invariant violated: buyer refund release failed");
                }
                const auto buyer_deposit_result = buyer_wallet.deposit(market.base(), execution.quantity);
                assert(buyer_deposit_result && "Invariant violated: buyer base deposit failed");

                const auto seller_consume_result = seller_wallet.consume_reserved(market.base(), execution.quantity);
                assert(seller_consume_result && "Invariant violated: seller reserved base must cover executed quantity");

                const auto seller_deposit_result = seller_wallet.deposit(market.quote(), execution.execution_price * execution.quantity);
                assert(seller_deposit_result && "Invariant violated: seller quote deposit failed");

                order_result.remaining_quantity -= execution.quantity;
                order_result.filled_quantity += execution.quantity;

                Trade trade{trade_id_generator_.next(), buyer_user_id, seller_user_id, buyer_order_id, seller_order_id, market, execution.quantity, execution.execution_price};

                trade_history_.add(std::move(trade));

                if (execution.buy_fully_filled)
                {
                    orders_.erase(execution.buy_order_id);
                    orders_market_.erase(execution.buy_order_id);
                }
                if (execution.sell_fully_filled)
                {
                    orders_.erase(execution.sell_order_id);
                    orders_market_.erase(execution.sell_order_id);
                }
            }
        }

        return order_result;
    }

    std::expected<CancelOrderResult, CancelOrderError> Exchange::cancel_order(const UserId user_id, const OrderId order_id)
    {

        if (users_.find(user_id) == users_.end())
            return std::unexpected(CancelOrderError::UserNotFound);

        auto order_it = orders_.find(order_id);

        if (order_it == orders_.end())
            return std::unexpected(CancelOrderError::OrderNotFound);

        if (order_it->second != user_id)
            return std::unexpected(CancelOrderError::NotOrderOwner);

        auto market_it = orders_market_.find(order_id);

        if (market_it == orders_market_.end())
        {
            orders_.erase(order_id);
            return std::unexpected(CancelOrderError::OrderNotFound);
        }

        auto cancel_result = matching_engine_.cancel(market_it->second, order_id);

        if (!cancel_result)
        {
            orders_.erase(order_id);
            orders_market_.erase(order_id);
            return std::unexpected(CancelOrderError::OrderNotFound);
        }
        auto &wallet = wallets_.find(user_id)->second;
        CancelOrderResult result;
        if (cancel_result->side == Side::Buy)
        {
            const auto buyer_release_result = wallet.release(market_it->second.quote(), cancel_result->remaining_quantity * cancel_result->price);
            assert(buyer_release_result && "Invariant violated: buyer release failed");
            result.side = Side::Buy;
        }
        else
        {
            const auto seller_release_result = wallet.release(market_it->second.base(), cancel_result->remaining_quantity);
            assert(seller_release_result && "Invariant violated: seller release failed");
            result.side = Side::Sell;
        }

        result.id=order_id;
        result.remaining_quantity = cancel_result->remaining_quantity;
        orders_.erase(order_id);
        orders_market_.erase(order_id);

        return result;
    }

    std::expected<void, RegisterMarketError> Exchange::register_market(const Market &market)
    {
        if (matching_engine_.has_market(market))
            return std::unexpected(RegisterMarketError::AlreadyListed);

        matching_engine_.register_market(market);

        return {};
    }
} // namespace vertex::application
