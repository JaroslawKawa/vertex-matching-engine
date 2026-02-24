#include <algorithm>
#include <cassert>
#include "vertex/engine/order_book.hpp"

namespace vertex::engine
{

    using Side = vertex::core::Side;

    OrderBook::OrderBook(Market market) : market_(market) {}

    std::vector<Execution> OrderBook::add_limit_order(std::unique_ptr<Order> order)
    {
        assert(order != nullptr);
        assert(order->market() == market_);

        OrderId order_id = order.get()->id();
        Price price = order.get()->price();
        Side side = order.get()->side();

        std::vector<Execution> result;

        if (side == Side::Buy)
        {

            while (order->remaining_quantity() > 0 && !asks_.empty() && asks_.begin()->first <= price)
            {
                auto level_it = asks_.begin(); // It for Prices/PriceLvl in asks_map

                auto &level = level_it->second;         // PriceLvl
                auto resting_it = level.orders.begin(); // it for begin of orders in PriceLvl list

                Order &resting = **resting_it; // Order

                Quantity executed = std::min(order->remaining_quantity(), resting.remaining_quantity());

                resting.reduce(executed);
                order->reduce(executed);

                result.push_back({order_id, resting.id(), executed, level_it->first, price, order->is_filled(), resting.is_filled()});

                if (resting.is_filled())
                {
                    index_.erase(resting.id());
                    level.orders.erase(resting_it);
                }

                if (level.orders.empty())
                {
                    asks_.erase(level_it);
                }
            }
            if (order.get()->is_active())
            {
                auto level_it = bids_.find(price);

                if (level_it == bids_.end())
                {
                    auto [it, inserted] = bids_.emplace(price, std::list<std::unique_ptr<Order>>{});
                    auto &orders = it->second.orders;
                    auto order_it = orders.insert(orders.end(), std::move(order));

                    index_[order_id] = {side, price, order_it};
                }
                else
                {
                    auto &orders = level_it->second.orders;
                    auto order_it = orders.insert(orders.end(), std::move(order));
                    index_[order_id] = {side, price, order_it};
                }
            }
        }
        else
        {
            while (order->remaining_quantity() > 0 && !bids_.empty() && bids_.begin()->first >= price)
            {

                auto level_it = bids_.begin(); // It for Prices/PriceLvl in bids list

                auto &level = level_it->second;         // Price Lvl
                auto resting_it = level.orders.begin(); // It for begin of orders in PriceLvl list

                Order &resting = **resting_it;

                Quantity executed = std::min(order->remaining_quantity(), resting.remaining_quantity());

                resting.reduce(executed);
                order->reduce(executed);

                result.push_back({resting.id(), order_id, executed, level_it->first, resting.price(), resting.is_filled(), order->is_filled()});

                if (resting.is_filled())
                {
                    index_.erase(resting.id());
                    level.orders.erase(resting_it);
                }

                if (level.orders.empty())
                {
                    bids_.erase(level_it);
                }
            }

            if (order.get()->is_active())
            {

                auto level_it = asks_.find(price);

                if (level_it == asks_.end())
                {

                    auto [it, inserted] = asks_.emplace(price, std::list<std::unique_ptr<Order>>{});
                    auto &orders = it->second.orders;
                    auto order_it = orders.insert(orders.end(), std::move(order));

                    index_[order_id] = {side, price, order_it};
                }
                else
                {
                    auto &orders = level_it->second.orders;
                    auto order_it = orders.insert(orders.end(), std::move(order));
                    index_[order_id] = {side, price, order_it};
                }
            }
        }
        return result;
    }

    std::vector<Execution> OrderBook::execute_market_order(std::unique_ptr<Order> order)
    {
        assert(order != nullptr);
        auto *market = dynamic_cast<MarketOrder *>(order.get());
        assert(market && "execute_market_order requires MarketOrder");
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

                Order &resting_order = **resting_it;

                auto executed_quantity = std::min(order->remaining_quantity(), resting_order.remaining_quantity());

                resting_order.reduce(executed_quantity);
                order->reduce(executed_quantity);

                result.push_back({order_id,
                                  resting_order.id(),
                                  executed_quantity,
                                  level_it->first,
                                  level_it->first,
                                  order->is_filled(),
                                  resting_order.is_filled()});

                if (resting_order.is_filled())
                {
                    index_.erase(resting_order.id());
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

                Order &resting_order = **resting_it;

                auto executed_quantity = std::min(order->remaining_quantity(), resting_order.remaining_quantity());

                resting_order.reduce(executed_quantity);
                order->reduce(executed_quantity);

                result.push_back({resting_order.id(),
                                  order->id(),
                                  executed_quantity,
                                  level_it->first,
                                  level_it->first,
                                  resting_order.is_filled(),
                                  order->is_filled()});

                if (resting_order.is_filled())
                {
                    index_.erase(resting_order.id());
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
        result.side = order_it->get()->side();
        result.price = order_it->get()->price();
        result.remaining_quantity = order_it->get()->remaining_quantity();

        if (order_it->get()->side() == Side::Buy)
        {
            auto level_it = bids_.find(order_it->get()->price());

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
            auto level_it = asks_.find(order_it->get()->price());

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

}