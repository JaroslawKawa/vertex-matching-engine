#include "vertex/application/exchange.hpp"

namespace vertex::application
{
    namespace
    {
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
    
    void Exchange::settle_trade(Account &buyer, Account &seller, const Execution &execution, const Market &market)
    {
        {
            auto locks = lock_two_accounts(buyer.user.id(), buyer, seller.user.id(), seller);
            const auto buyer_consume_result = buyer.wallet.consume_reserved(market.quote(), execution.execution_price * execution.quantity);
            assert(buyer_consume_result && "Invariant violated: buyer reserved quote must cover executed notional");

            if (execution.buy_order_limit_price)
            {
                Quantity refund = execution.buy_order_limit_price.value() * execution.quantity - execution.execution_price * execution.quantity;
                if (0 < refund)
                {
                    const auto buyer_release_result = buyer.wallet.release(market.quote(), refund);
                    assert(buyer_release_result && "Invariant violated: buyer refund release failed");
                }
            }
            const auto buyer_deposit_result = buyer.wallet.deposit(market.base(), execution.quantity);
            assert(buyer_deposit_result && "Invariant violated: buyer base deposit failed");

            const auto seller_consume_result = seller.wallet.consume_reserved(market.base(), execution.quantity);
            assert(seller_consume_result && "Invariant violated: seller reserved base must cover executed quantity");
            const auto seller_deposit_result = seller.wallet.deposit(market.quote(), execution.execution_price * execution.quantity);
            assert(seller_deposit_result && "Invariant violated: seller quote deposit failed");
        }
    }

    std::shared_ptr<Account> Exchange::get_account(UserId id) const
    {
        std::shared_lock lock(accounts_mu_);
        auto account_it = accounts_.find(id);
        if (account_it == accounts_.end())
            return nullptr;
        return account_it->second;
    }

    std::pair<std::shared_ptr<Account>, std::shared_ptr<Account>> Exchange::get_accounts(UserId id_1, UserId id_2) const
    {
        std::pair<std::shared_ptr<Account>, std::shared_ptr<Account>> result;
        std::shared_lock lock(accounts_mu_);
        auto account_it_1 = accounts_.find(id_1);
        if (account_it_1 != accounts_.end())
            result.first = account_it_1->second;
        auto account_it_2 = accounts_.find(id_2);
        if (account_it_2 != accounts_.end())
            result.second = account_it_2->second;
        return result;
    }

    void Exchange::rollback_release_or_assert(Account &account, const Asset &asset, const Quantity quantity, const std::string &context)
    {
        std::expected<void, WalletError> rollback_release_result;
        {
            std::lock_guard lock(account.mu);
            rollback_release_result = account.wallet.release(asset, quantity);
        }
        assert(rollback_release_result && context.c_str());
    }

}