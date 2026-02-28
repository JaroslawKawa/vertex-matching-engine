#include <algorithm>
#include <cassert>
#include "vertex/engine/order_book.hpp"

namespace vertex::engine
{

    using Side = vertex::core::Side;

    OrderBook::OrderBook(Market market) : market_(market) {}

    std::vector<Execution> OrderBook::execute_market_order(std::unique_ptr<MarketOrder> order)
    {
        assert(order != nullptr);
        assert(order->market() == market_);

        OrderId order_id = order->id();
        Side side = order->side();

        std::vector<Execution> result;

        if (side == Side::Buy)
        {

            while (order->remaining_quantity() > 0 && !asks_.empty())
            {
                auto level_it = asks_.begin();
                auto &level = level_it->second;
                auto resting_it = level.orders.begin();

                RestingOrder &resting_order = *resting_it;
                Price price = level_it->first;

                auto remaining_quote = order->remaining_quantity(); // quote budget remaining
                auto max_base = remaining_quote / price;            // how much base we can buy at this price
                auto executed_base = std::min(max_base, resting_order.remaining_base_quantity);

                // intiger math gouard
                if (executed_base <= 0)
                    break;

                resting_order.reduce(executed_base);
                order->reduce(executed_base * price);

                result.push_back({order_id,
                                  resting_order.order_id,
                                  executed_base,
                                  price,
                                  price,
                                  order->is_filled(),
                                  resting_order.is_filled()});

                if (resting_order.is_filled())
                {
                    index_.erase(resting_order.order_id);
                    level.orders.erase(resting_it);
                }
                if (level.orders.empty())
                {
                    asks_.erase(level_it);
                }
            }
        }
        else
        {
            while (order->remaining_quantity() > 0 && !bids_.empty())
            {
                auto level_it = bids_.begin();
                auto &level = level_it->second;
                auto resting_it = level.orders.begin();

                RestingOrder &resting_order = *resting_it;
                Price price = level_it->first;

                auto executed_quantity = std::min(order->remaining_quantity(), resting_order.remaining_base_quantity);

                resting_order.reduce(executed_quantity);
                order->reduce(executed_quantity);

                result.push_back({resting_order.order_id,
                                  order->id(),
                                  executed_quantity,
                                  price,
                                  price,
                                  resting_order.is_filled(),
                                  order->is_filled()});

                if (resting_order.is_filled())
                {
                    index_.erase(resting_order.order_id);
                    level.orders.erase(resting_it);
                }
                if (level.orders.empty())
                {
                    bids_.erase(level_it);
                }
            }
        }

        return result;
    }

    std::optional<CancelResult> OrderBook::cancel(OrderId order_id)
    {
        assert(order_id.is_valid());

        auto order_location_it = index_.find(order_id);

        if (order_location_it == index_.end())
        {
            return std::nullopt;
        }

        CancelResult result;

        auto order_it = order_location_it->second.it;

        result.id = order_id;
        result.side = order_location_it->second.side;
        result.price = order_it->limit_price;
        result.remaining_quantity = order_it->remaining_base_quantity;

        if (order_location_it->second.side == Side::Buy)
        {
            auto level_it = bids_.find(order_it->limit_price);

            if (level_it != bids_.end())
            {
                level_it->second.orders.erase(order_it);
                if (level_it->second.orders.empty())
                {
                    bids_.erase(level_it);
                }
            }
        }
        else
        {
            auto level_it = asks_.find(order_it->limit_price);

            if (level_it != asks_.end())
            {
                level_it->second.orders.erase(order_it);
                if (level_it->second.orders.empty())
                {
                    asks_.erase(level_it);
                }
            }
        }

        index_.erase(order_location_it);
        return result;
    }

    std::optional<Price> OrderBook::best_bid() const
    {

        if (bids_.empty())
            return std::nullopt;

        return bids_.begin()->first;
    }

    std::optional<Price> OrderBook::best_ask() const
    {

        if (asks_.empty())
            return std::nullopt;

        return asks_.begin()->first;
    }

    void OrderBook::insert_resting(Side side, RestingOrder &&order)
    {

        if (side == Side::Buy)
        {
            auto level_it = bids_.find(order.limit_price);

            if (level_it == bids_.end())
            {
                auto [it, inserted] = bids_.try_emplace(order.limit_price);
                auto &orders = it->second.orders;
                auto order_it = orders.insert(orders.end(), std::move(order));

                index_[order.order_id] = {side, order.limit_price, order_it};
            }
            else
            {
                auto &orders = level_it->second.orders;
                auto order_it = orders.insert(orders.end(), std::move(order));
                index_[order.order_id] = {side, order.limit_price, order_it};
            }
        }
        else
        {
            auto level_it = asks_.find(order.limit_price);

            if (level_it == asks_.end())
            {

                auto [it, inserted] = asks_.try_emplace(order.limit_price);
                auto &orders = it->second.orders;
                auto order_it = orders.insert(orders.end(), std::move(order));

                index_[order.order_id] = {side, order.limit_price, order_it};
            }
            else
            {
                auto &orders = level_it->second.orders;
                auto order_it = orders.insert(orders.end(), std::move(order));
                index_[order.order_id] = {side, order.limit_price, order_it};
            }
        }
    }

    std::vector<Execution> OrderBook::match_limit_buy_against_asks(const OrderId taker_order_id, const Price limit_price, Quantity &remaining_base_quantity)
    {
        std::vector<Execution> result;

        while (remaining_base_quantity > 0 && !asks_.empty() && asks_.begin()->first <= limit_price)
        {
            auto &level = asks_.begin()->second; // PriceLvl
            Price price = asks_.begin()->first;

            auto resting_it = level.orders.begin(); // it for begin of orders in PriceLvl list
            RestingOrder &resting = *resting_it;    // Order

            Quantity executed = std::min(remaining_base_quantity, resting.remaining_base_quantity);

            resting.reduce(executed);
            remaining_base_quantity -= executed;

            bool order_filled = remaining_base_quantity == 0 ? true : false;

            result.push_back({taker_order_id, resting.order_id, executed, price, limit_price, order_filled, resting.is_filled()});

            if (resting.is_filled())
            {
                index_.erase(resting.order_id);
                level.orders.erase(resting_it);
            }

            if (level.orders.empty())
            {
                asks_.erase(asks_.begin());
            }
        }

        return result;
    }

    std::vector<Execution> OrderBook::match_limit_sell_against_bids(const OrderId taker_order_id, const Price limit_price, Quantity &remaining_base_quantity)
    {
        std::vector<Execution> result;

        while (remaining_base_quantity > 0 && !bids_.empty() && bids_.begin()->first >= limit_price)
        {
            auto &level = bids_.begin()->second; // Price Lvl
            Price price = bids_.begin()->first;

            auto resting_it = level.orders.begin(); // It for begin of orders in PriceLvl list
            RestingOrder &resting = *resting_it;

            Quantity executed = std::min(remaining_base_quantity, resting.remaining_base_quantity);

            resting.reduce(executed);
            remaining_base_quantity -= executed;

            bool order_filled = remaining_base_quantity == 0 ? true : false;

            result.push_back({resting.order_id, taker_order_id, executed, price, resting.limit_price, resting.is_filled(), order_filled});

            if (resting.is_filled())
            {
                index_.erase(resting.order_id);
                level.orders.erase(resting_it);
            }

            if (level.orders.empty())
            {
                bids_.erase(bids_.begin());
            }
        }
        return result;
    }

}