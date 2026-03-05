#include "vertex/application/exchange.hpp"

namespace vertex::application
{
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

    }

    std::optional<PlaceOrderError> Exchange::validate_order(const UserId user_id, const Market &market, const std::optional<Price> price, const Quantity quantity) const
    {
        if (!user_id.is_valid())
            return PlaceOrderError::UserNotFound;

        if (!market_dispatcher_.has_market(market))
            return PlaceOrderError::MarketNotListed;

        if (quantity <= 0)
            return PlaceOrderError::InvalidQuantity;

        if (price)
        {
            if (price <= 0)
                return PlaceOrderError::InvalidAmount;
        }
        return std::nullopt;
    }

    std::expected<OrderPlacementResult, PlaceOrderError> Exchange::place_limit_order(const UserId user_id, const Market &market, const Side side, const Price price, const Quantity quantity)
    {
        auto order_validation_error = validate_order(user_id, market, price, quantity);
        if (order_validation_error)
            return std::unexpected(order_validation_error.value());

        auto prepared_limit_order = prepare_and_reserve_limit_order(user_id, market, side, price, quantity);

        if (!prepared_limit_order)
            return std::unexpected(prepared_limit_order.error());

        Asset asset_to_reserve = std::move(prepared_limit_order->asset_to_reserve);
        Quantity quantity_to_reserve = prepared_limit_order->quantity_to_reserve;
        std::shared_ptr<Account> account = std::move(prepared_limit_order->account);
        OrderId order_id = prepared_limit_order->id;
        LimitOrderRequest limit_order_request = std::move(prepared_limit_order->order_request);

        OrderPlacementResult order_result;
        order_result.order_id = limit_order_request.id;
        order_result.remaining_quantity = quantity;
        order_result.filled_quantity = 0;

        if (!order_meta_store_.try_insert(order_id, std::move(prepared_limit_order->meta)))
        {
            rollback_release_or_assert(*account, asset_to_reserve, quantity_to_reserve, "Invariant violated: rollback release failed after limit submit error");
            return std::unexpected(PlaceOrderError::OrderIdCollision);
        }

        auto matching_result = market_dispatcher_.submit(std::move(limit_order_request)).get();

        if (!matching_result)
        {
            rollback_release_or_assert(*account, asset_to_reserve, quantity_to_reserve, "Invariant violated: rollback release failed after limit submit error");
            order_meta_store_.erase(order_id);
            return std::unexpected(map_to_place_order_error(matching_result.error()));
        }

        for (const Execution &execution : matching_result.value())
        {

            OrderId buyer_order_id = execution.buy_order_id;
            OrderId seller_order_id = execution.sell_order_id;

            auto buyer_order_meta = order_meta_store_.find(buyer_order_id);
            assert(buyer_order_meta != std::nullopt);
            UserId buyer_user_id = buyer_order_meta->owner;

            auto seller_order_meta = order_meta_store_.find(seller_order_id);
            assert(seller_order_meta != std::nullopt);
            UserId seller_user_id = seller_order_meta->owner;

            auto [buyer, seller] = get_accounts(buyer_user_id, seller_user_id);
            assert((buyer != nullptr && seller != nullptr) && "Invariant violated: buyer or seller not exist");

            settle_trade(*buyer, *seller, execution, market);

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
                auto record = order_meta_store_.close_and_extract(execution.buy_order_id, OrderStatus::Filled);
                if (record)
                    order_history_.try_insert(std::move(record.value()));
            }
            if (execution.sell_fully_filled)
            {
                auto record = order_meta_store_.close_and_extract(execution.sell_order_id, OrderStatus::Filled);
                if (record)
                    order_history_.try_insert(std::move(record.value()));
            }
        }

        return order_result;
    }

    std::expected<OrderPlacementResult, PlaceOrderError> Exchange::execute_market_order(const UserId user_id, const Market &market, const Side side, const Quantity order_quantity)
    {
        auto order_validation_error = validate_order(user_id, market, std::nullopt, order_quantity);

        if (order_validation_error)
            return std::unexpected(order_validation_error.value());

        Asset asset_to_reserve = (side == Side::Buy) ? market.quote() : market.base();

        std::shared_ptr<Account> account = get_account(user_id);
        if (account == nullptr)
            return std::unexpected(PlaceOrderError::UserNotFound);

        std::expected<void, WalletError> reserve_result;
        {
            std::lock_guard lock(account->mu);
            reserve_result = account->wallet.reserve(asset_to_reserve, order_quantity);
        }
        if (!reserve_result)
            return std::unexpected(PlaceOrderError::InsufficientFunds);

        std::expected<OrderPlacementResult, PlaceOrderError> order_result;

        if (Side::Buy == side)
            order_result = execute_market_buy_by_quote(user_id, market, order_quantity);
        else
            order_result = execute_market_sell_by_base(user_id, market, order_quantity);

        if (!order_result)
            rollback_release_or_assert(*account, asset_to_reserve, order_quantity, "Invariant violated: rollback release failed after market submit error");

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

        std::shared_ptr<Account> buyer = get_account(user_id);
        if (buyer == nullptr)
            return std::unexpected(PlaceOrderError::UserNotFound);

        OrderRecord taker_record{.id = order_result.order_id,
                                 .user_id = buyer->user.id(),
                                 .market = market,
                                 .side = Side::Buy,
                                 .type = OrderType::MarketOrder,
                                 .status = OrderStatus::Filled,
                                 .requested_quote_budget = order_quantity};

        for (const auto &execution : execution_result)
        {
            OrderId seller_order_id = execution.sell_order_id;
            auto seller_order_meta = order_meta_store_.find(seller_order_id);
            assert(seller_order_meta != std::nullopt);
            UserId seller_user_id = seller_order_meta->owner;

            std::shared_ptr<Account> seller = get_account(seller_user_id);
            assert(seller != nullptr && "Invariant violated: seller not exist");

            settle_trade(*buyer, *seller, execution, market);

            order_result.remaining_quantity -= execution.quantity * execution.execution_price;
            order_result.filled_quantity += execution.quantity * execution.execution_price;

            taker_record.executed_base_qty += execution.quantity;
            taker_record.executed_quote_qty += execution.quantity * execution.execution_price;
            taker_record.fill_count += 1;

            TradeId trade_id;
            {
                std::lock_guard lock(trade_id_generator_mu_);
                trade_id = trade_id_generator_.next();
            }

            Trade trade{trade_id, user_id, seller_user_id, order_result.order_id, seller_order_id, market, execution.quantity, execution.execution_price};

            order_meta_store_.append_fill(seller_order_id, trade_id, execution.quantity, execution.execution_price);
            taker_record.trade_ids.push_back(trade_id);

            trade_history_.add(std::move(trade));

            if (execution.sell_fully_filled)
            {
                auto record = order_meta_store_.close_and_extract(execution.sell_order_id, OrderStatus::Filled);
                if (record)
                    order_history_.try_insert(std::move(record.value()));
            }
        }

        std::optional<double> avg_price;
        if (taker_record.executed_base_qty != 0)
        {
            avg_price = std::optional<double>(static_cast<double>(taker_record.executed_quote_qty) / taker_record.executed_base_qty);
        }
        taker_record.avg_price = avg_price;

        if (order_result.remaining_quantity > 0)
        {
            if (order_result.filled_quantity == 0)
            {
                taker_record.status = OrderStatus::Unfilled;
            }
            else
            {
                taker_record.status = OrderStatus::PartiallyFilled;
            }
            std::lock_guard lock(buyer->mu);
            const auto taker_release_result = buyer->wallet.release(market.quote(), order_result.remaining_quantity);
            assert(taker_release_result && "Invariant violated: taker release failed after market buy");
        }

        order_history_.try_insert(std::move(taker_record));

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

        std::shared_ptr<Account> seller = get_account(user_id);
        if (seller == nullptr)
            return std::unexpected(PlaceOrderError::UserNotFound);

        OrderRecord taker_record{.id = order_result.order_id,
                                 .user_id = seller->user.id(),
                                 .market = market,
                                 .side = Side::Sell,
                                 .type = OrderType::MarketOrder,
                                 .status = OrderStatus::Filled,
                                 .requested_base_qty = order_quantity};

        for (const auto &execution : execution_result)
        {
            OrderId buyer_order_id = execution.buy_order_id;

            auto buyer_order_meta = order_meta_store_.find(buyer_order_id);
            assert(buyer_order_meta != std::nullopt);
            UserId buyer_user_id = buyer_order_meta->owner;

            std::shared_ptr<Account> buyer = get_account(buyer_user_id);
            assert(buyer != nullptr && "Invariant violated: buyer not exist");

            settle_trade(*buyer, *seller, execution, market);

            order_result.remaining_quantity -= execution.quantity;
            order_result.filled_quantity += execution.quantity;

            taker_record.executed_base_qty += execution.quantity;
            taker_record.executed_quote_qty += execution.quantity * execution.execution_price;
            taker_record.fill_count += 1;
            TradeId trade_id;
            {
                std::lock_guard lock(trade_id_generator_mu_);
                trade_id = trade_id_generator_.next();
            }
            Trade trade{trade_id, buyer_user_id, user_id, buyer_order_id, order_result.order_id, market, execution.quantity, execution.execution_price};

            taker_record.trade_ids.push_back(trade_id);
            order_meta_store_.append_fill(buyer_order_id, trade_id, execution.quantity, execution.execution_price);

            trade_history_.add(std::move(trade));

            if (execution.buy_fully_filled)
            {
                auto record = order_meta_store_.close_and_extract(execution.buy_order_id, OrderStatus::Filled);
                if (record)
                    order_history_.try_insert(std::move(record.value()));
            }
        }

        std::optional<double> avg_price;
        if (taker_record.executed_base_qty != 0)
        {
            avg_price = std::optional<double>(static_cast<double>(taker_record.executed_quote_qty) / taker_record.executed_base_qty);
        }

        taker_record.avg_price = avg_price;

        if (order_result.remaining_quantity > 0)
        {
            if (order_result.filled_quantity == 0)
            {
                taker_record.status = OrderStatus::Unfilled;
            }
            else
            {
                taker_record.status = OrderStatus::PartiallyFilled;
            }
            std::lock_guard lock(seller->mu);
            const auto taker_release_result = seller->wallet.release(market.base(), order_result.remaining_quantity);
            assert(taker_release_result && "Invariant violated: taker release failed after market sell");
        }

        order_history_.try_insert(std::move(taker_record));

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

        std::shared_ptr<Account> account = get_account(user_id);
        if (account == nullptr)
            return std::unexpected(CancelOrderError::UserNotFound);

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

        auto record = order_meta_store_.close_and_extract(order_id, OrderStatus::Canceled);
        if (record)
            order_history_.try_insert(std::move(record.value()));

        result.id = order_id;
        result.remaining_quantity = cancel_result->remaining_quantity;
        return result;
    }

    std::expected<Exchange::PreparedLimitOrder, PlaceOrderError> Exchange::prepare_and_reserve_limit_order(const UserId &user_id, const Market &market, const Side &side, const Price &price, const Quantity &quantity)
    {

        Asset asset_to_reserve = (side == Side::Buy) ? market.quote() : market.base();
        Quantity quantity_to_reserve = (side == Side::Buy) ? price * quantity : quantity;

        std::shared_ptr<Account> account = get_account(user_id);
        if (account == nullptr)
            return std::unexpected(PlaceOrderError::UserNotFound);

        std::expected<void, WalletError> reserve_result;
        {
            std::lock_guard lock(account->mu);
            reserve_result = account->wallet.reserve(asset_to_reserve, quantity_to_reserve);
        }
        if (!reserve_result)
            return std::unexpected(PlaceOrderError::InsufficientFunds);

        OrderId id;
        {
            std::lock_guard lock(order_id_generator_mu_);
            id = order_id_generator_.next();
        }

        LimitOrderRequest limit_order_request{
            .id = id,
            .user_id = user_id,
            .market = market,
            .side = side,
            .limit_price = price,
            .base_quantity = quantity};

        OrderMeta meta{
            .owner = user_id,
            .market = market,
            .side = side,
            .price = price,
            .requested_base_qty = quantity};

        return PreparedLimitOrder{
            .account = std::move(account),
            .asset_to_reserve = std::move(asset_to_reserve),
            .quantity_to_reserve = quantity_to_reserve,
            .id = id,
            .order_request = std::move(limit_order_request),
            .meta = std::move(meta)};
    }
}