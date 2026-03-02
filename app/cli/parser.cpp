#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstddef>
#include <vector>
#include "parser.hpp"
#include "tokenizer.hpp"

namespace vertex::cli
{

    namespace
    {
        std::expected<std::int64_t, std::errc> parse_int64(std::string_view str)
        {
            std::int64_t result{};
            const char *begin = str.data();
            const char *end = str.data() + str.size();
            auto [ptr, ec] = std::from_chars(begin, end, result);

            if (ec == std::errc() && ptr == end)
                return result;
            if (ec == std::errc() && ptr != end)
                return std::unexpected(std::errc::invalid_argument);

            return std::unexpected(ec);
        }

        std::expected<std::uint64_t, std::errc> parse_uint64(std::string_view str)
        {
            std::uint64_t result{};
            const char *begin = str.data();
            const char *end = str.data() + str.size();
            auto [ptr, ec] = std::from_chars(begin, end, result);

            if (ec == std::errc() && ptr == end)
                return result;
            if (ec == std::errc() && ptr != end)
                return std::unexpected(std::errc::invalid_argument);

            return std::unexpected(ec);
        }

        std::expected<void, ParseError> validate_arguments_count(const std::vector<Token> &tokens, std::size_t count)
        {
            if (tokens.size() < count)
            {
                const std::size_t col = tokens.empty() ? 0 : tokens[0].index;
                return std::unexpected(ParseError{
                    .stage = ParseStage::Parser,
                    .code = ParseErrorCode::MissingArgument,
                    .message = "Missing argument",
                    .column = col});
            }
            if (tokens.size() > count)
            {
                return std::unexpected(ParseError{
                    .stage = ParseStage::Parser,
                    .code = ParseErrorCode::TooManyArguments,
                    .message = "Too many arguments",
                    .column = tokens[count].index});
            }
            return {};
        }

        std::expected<std::string_view, ParseError> validate_name(std::string_view str, std::size_t col)
        {
            auto name_condition = ([](const char c)
                                   { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == ' '; });

            auto name_ok = std::ranges::all_of(str, name_condition);

            if (!name_ok)
                return std::unexpected(ParseError{
                    .stage = ParseStage::Parser,
                    .code = ParseErrorCode::InvalidName,
                    .message = "A name must contain only alphabetic characters and spaces",
                    .column = col});

            return {str};
        }

        std::expected<uint64_t, ParseError> validate_user_id(std::string_view str, std::size_t col)
        {
            auto user_id = parse_uint64(str);

            if (!user_id)
            {
                if (user_id.error() == std::errc::invalid_argument)
                {
                    return std::unexpected(ParseError{
                        .stage = ParseStage::Parser,
                        .code = ParseErrorCode::InvalidId,
                        .message = "User id must contain only digits",
                        .column = col});
                }
                if (user_id.error() == std::errc::result_out_of_range)
                {
                    return std::unexpected(ParseError{
                        .stage = ParseStage::Parser,
                        .code = ParseErrorCode::InvalidId,
                        .message = "UserId is larger than a uint64",
                        .column = col});
                }
                return std::unexpected(ParseError{
                    .stage = ParseStage::Parser,
                    .code = ParseErrorCode::InvalidId,
                    .message = "Invalid user id",
                    .column = col});
            }
            return user_id.value();
        }

        std::expected<uint64_t, ParseError> validate_order_id(std::string_view str, std::size_t col)
        {
            auto order_id = parse_uint64(str);

            if (!order_id)
            {
                if (order_id.error() == std::errc::invalid_argument)
                {
                    return std::unexpected(ParseError{
                        .stage = ParseStage::Parser,
                        .code = ParseErrorCode::InvalidId,
                        .message = "Order id must contain only digits",
                        .column = col});
                }
                if (order_id.error() == std::errc::result_out_of_range)
                {
                    return std::unexpected(ParseError{
                        .stage = ParseStage::Parser,
                        .code = ParseErrorCode::InvalidId,
                        .message = "Order id is larger than a uint64",
                        .column = col});
                }
                return std::unexpected(ParseError{
                    .stage = ParseStage::Parser,
                    .code = ParseErrorCode::InvalidId,
                    .message = "Invalid order id",
                    .column = col});
            }
            return order_id.value();
        }

        std::expected<std::string_view, ParseError> validate_asset(std::string_view str, std::size_t col)
        {
            if (str.size() < 3 || str.size() > 10)
            {
                return std::unexpected(ParseError{
                    .stage = ParseStage::Parser,
                    .code = ParseErrorCode::InvalidAsset,
                    .message = "Asset must contain 3-10 letters",
                    .column = col});
            }

            auto asset_condition = ([](const char c)
                                    { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); });

            auto asset_ok = std::ranges::all_of(str, asset_condition);

            if (!asset_ok)
                return std::unexpected(ParseError{
                    .stage = ParseStage::Parser,
                    .code = ParseErrorCode::InvalidAsset,
                    .message = "Asset must contain only A-Z letters",
                    .column = col});

            return {str};
        }

        std::expected<std::int64_t, ParseError> validate_quantity(std::string_view str, std::size_t col)
        {
            auto quantity = parse_int64(str);

            if (!quantity)
            {
                if (quantity.error() == std::errc::invalid_argument)
                {
                    return std::unexpected(ParseError{
                        .stage = ParseStage::Parser,
                        .code = ParseErrorCode::InvalidNumber,
                        .message = "Quantity must contain only digits",
                        .column = col});
                }
                if (quantity.error() == std::errc::result_out_of_range)
                {
                    return std::unexpected(ParseError{
                        .stage = ParseStage::Parser,
                        .code = ParseErrorCode::InvalidNumber,
                        .message = "Quantity is larger than a int64",
                        .column = col});
                }
                return std::unexpected(ParseError{
                    .stage = ParseStage::Parser,
                    .code = ParseErrorCode::InvalidNumber,
                    .message = "Invalid quantity",
                    .column = col});
            }
            return quantity.value();
        }

        std::expected<std::int64_t, ParseError> validate_price(std::string_view str, std::size_t col)
        {
            auto price = parse_int64(str);

            if (!price)
            {
                if (price.error() == std::errc::invalid_argument)
                {
                    return std::unexpected(ParseError{
                        .stage = ParseStage::Parser,
                        .code = ParseErrorCode::InvalidNumber,
                        .message = "Price must contain only digits",
                        .column = col});
                }
                if (price.error() == std::errc::result_out_of_range)
                {
                    return std::unexpected(ParseError{
                        .stage = ParseStage::Parser,
                        .code = ParseErrorCode::InvalidNumber,
                        .message = "Price is larger than a int64",
                        .column = col});
                }
                return std::unexpected(ParseError{
                    .stage = ParseStage::Parser,
                    .code = ParseErrorCode::InvalidNumber,
                    .message = "Invalid price",
                    .column = col});
            }
            return price.value();
        }

        std::expected<std::string_view, ParseError> validate_market(std::string_view str, std::size_t col)
        {
            auto iequals_ascii = [](std::string_view a, std::string_view b) -> bool
            {
                if (a.size() != b.size())
                    return false;

                return std::ranges::equal(
                    a, b,
                    [](char lhs, char rhs)
                    {
                        return std::tolower(static_cast<unsigned char>(lhs)) ==
                               std::tolower(static_cast<unsigned char>(rhs));
                    });
            };

            auto slash_it = str.find('/');

            if (slash_it == str.npos)
            {
                return std::unexpected(ParseError{
                    .stage = ParseStage::Parser,
                    .code = ParseErrorCode::InvalidMarket,
                    .message = "Market must be in format <base>/<quote>",
                    .column = col});
            }

            auto scnd_slash = str.find('/', slash_it + 1);
            if (scnd_slash != str.npos)
            {
                return std::unexpected(ParseError{
                    .stage = ParseStage::Parser,
                    .code = ParseErrorCode::InvalidMarket,
                    .message = "Market must be in format <base>/<quote>",
                    .column = col});
            }

            auto base = validate_asset(str.substr(0, slash_it), col);
            if (!base)
            {
                return std::unexpected(base.error());
            }

            auto quote = validate_asset(str.substr(slash_it + 1), col + slash_it + 1);
            if (!quote)
            {
                return std::unexpected(quote.error());
            }

            if (iequals_ascii(base.value(), quote.value()))
            {
                return std::unexpected(ParseError{
                    .stage = ParseStage::Parser,
                    .code = ParseErrorCode::InvalidMarket,
                    .message = "Market base and quote must be different assets",
                    .column = col});
            }

            return str;
        }

        std::expected<std::string_view, ParseError> validate_side(std::string_view str, std::size_t col)
        {

            std::string s(str);

            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
                           { return static_cast<char>(std::tolower(c)); });

            if ((s == "buy") || (s == "sell"))
            {
                return str;
            }
            else
                return std::unexpected(ParseError{.stage = ParseStage::Parser,
                                                  .code = ParseErrorCode::InvalidSide,
                                                  .message = "Side must be buy or sell",
                                                  .column = col});
        }

        std::expected<CreateUser, ParseError> parse_create_user(const std::vector<Token> &tokens)
        {
            auto count_ok = validate_arguments_count(tokens, 2);

            if (!count_ok)
                return std::unexpected(count_ok.error());

            auto name = validate_name(tokens[1].text, tokens[1].index);

            if (!name)
                return std::unexpected(name.error());

            return CreateUser{.name = std::string(name.value())};
        }

        std::expected<GetUser, ParseError> parse_user_get(const std::vector<Token> &tokens)
        {
            auto count_ok = validate_arguments_count(tokens, 2);

            if (!count_ok)
            {
                return std::unexpected(count_ok.error());
            }

            auto user_id = validate_user_id(tokens[1].text, tokens[1].index);

            if (!user_id)
            {
                return std::unexpected(user_id.error());
            }
            return GetUser{.user_id = user_id.value()};
        }

        std::expected<WalletDeposit, ParseError> parse_wallet_deposit(const std::vector<Token> &tokens)
        {

            auto count_ok = validate_arguments_count(tokens, 4);
            if (!count_ok)
                return std::unexpected(count_ok.error());

            auto user_id = validate_user_id(tokens[1].text, tokens[1].index);
            if (!user_id)
                return std::unexpected(user_id.error());

            auto asset = validate_asset(tokens[2].text, tokens[2].index);
            if (!asset)
                return std::unexpected(asset.error());

            auto quantity = validate_quantity(tokens[3].text, tokens[3].index);
            if (!quantity)
                return std::unexpected(quantity.error());

            return WalletDeposit{.user_id = user_id.value(),
                                 .asset = std::string(asset.value()),
                                 .quantity = quantity.value()};
        }

        std::expected<WalletWithdraw, ParseError> parse_wallet_withdraw(const std::vector<Token> &tokens)
        {

            auto count_ok = validate_arguments_count(tokens, 4);
            if (!count_ok)
                return std::unexpected(count_ok.error());

            auto user_id = validate_user_id(tokens[1].text, tokens[1].index);
            if (!user_id)
                return std::unexpected(user_id.error());

            auto asset = validate_asset(tokens[2].text, tokens[2].index);
            if (!asset)
                return std::unexpected(asset.error());

            auto quantity = validate_quantity(tokens[3].text, tokens[3].index);
            if (!quantity)
                return std::unexpected(quantity.error());

            return WalletWithdraw{.user_id = user_id.value(),
                                  .asset = std::string(asset.value()),
                                  .quantity = quantity.value()};
        }

        std::expected<WalletFreeBalance, ParseError> parse_wallet_free_balance(const std::vector<Token> &tokens)
        {
            auto count_ok = validate_arguments_count(tokens, 3);
            if (!count_ok)
                return std::unexpected(count_ok.error());

            auto user_id = validate_user_id(tokens[1].text, tokens[1].index);
            if (!user_id)
                return std::unexpected(user_id.error());

            auto asset = validate_asset(tokens[2].text, tokens[2].index);
            if (!asset)
                return std::unexpected(asset.error());

            return WalletFreeBalance{
                .user_id = user_id.value(),
                .asset = std::string(asset.value())};
        }

        std::expected<WalletReservedBalance, ParseError> parse_wallet_reserved_balance(const std::vector<Token> &tokens)
        {
            auto count_ok = validate_arguments_count(tokens, 3);
            if (!count_ok)
                return std::unexpected(count_ok.error());

            auto user_id = validate_user_id(tokens[1].text, tokens[1].index);
            if (!user_id)
                return std::unexpected(user_id.error());

            auto asset = validate_asset(tokens[2].text, tokens[2].index);
            if (!asset)
                return std::unexpected(asset.error());

            return WalletReservedBalance{
                .user_id = user_id.value(),
                .asset = std::string(asset.value())};
        }

        std::expected<PlaceLimitOrder, ParseError> parse_place_limit_order(const std::vector<Token> &tokens)
        {

            auto count_ok = validate_arguments_count(tokens, 6);
            if (!count_ok)
                return std::unexpected(count_ok.error());

            auto user_id = validate_user_id(tokens[1].text, tokens[1].index);
            if (!user_id)
                return std::unexpected(user_id.error());

            auto market = validate_market(tokens[2].text, tokens[2].index);
            if (!market)
                return std::unexpected(market.error());

            auto side = validate_side(tokens[3].text, tokens[3].index);
            if (!side)
                return std::unexpected(side.error());

            auto price = validate_price(tokens[4].text, tokens[4].index);
            if (!price)
                return std::unexpected(price.error());

            auto quantity = validate_quantity(tokens[5].text, tokens[5].index);
            if (!quantity)
                return std::unexpected(quantity.error());

            return PlaceLimitOrder{.user_id = user_id.value(),
                                   .market = std::string(market.value()),
                                   .side = std::string(side.value()),
                                   .price = price.value(),
                                   .quantity = quantity.value()};
        }

        std::expected<ExecuteMarketOrder, ParseError> parse_execute_market_order(const std::vector<Token> &tokens)
        {

            auto count_ok = validate_arguments_count(tokens, 5);
            if (!count_ok)
                return std::unexpected(count_ok.error());

            auto user_id = validate_user_id(tokens[1].text, tokens[1].index);
            if (!user_id)
                return std::unexpected(user_id.error());

            auto market = validate_market(tokens[2].text, tokens[2].index);
            if (!market)
                return std::unexpected(market.error());

            auto side = validate_side(tokens[3].text, tokens[3].index);
            if (!side)
                return std::unexpected(side.error());

            auto quantity = validate_quantity(tokens[4].text, tokens[4].index);
            if (!quantity)
                return std::unexpected(quantity.error());

            return ExecuteMarketOrder{.user_id = user_id.value(),
                                      .market = std::string(market.value()),
                                      .side = std::string(side.value()),
                                      .quantity = quantity.value()};
        }

        std::expected<CancelOrder, ParseError> parse_cancel_order(const std::vector<Token> &tokens)
        {

            auto count_ok = validate_arguments_count(tokens, 3);
            if (!count_ok)
                return std::unexpected(count_ok.error());

            auto user_id = validate_user_id(tokens[1].text, tokens[1].index);
            if (!user_id)
                return std::unexpected(user_id.error());

            auto order_id = validate_order_id(tokens[2].text, tokens[2].index);
            if (!order_id)
                return std::unexpected(order_id.error());

            return CancelOrder{.user_id = user_id.value(),
                               .order_id = order_id.value()};
        }

        std::expected<RegisterMarket, ParseError> parse_register_market(const std::vector<Token> &tokens)
        {

            auto count_ok = validate_arguments_count(tokens, 2);
            if (!count_ok)
                return std::unexpected(count_ok.error());

            auto market = validate_market(tokens[1].text, tokens[1].index);
            if (!market)
                return std::unexpected(market.error());

            return RegisterMarket{.market = std::string(market.value())};
        }
    }
    std::expected<Command, ParseError> parse_command(std::string_view line)
    {
        auto tokens = tokenize(line);

        if (!tokens)
            return std::unexpected(tokens.error());

        const auto &t = *tokens;

        auto root = t[0].text;

        if (root == "help")
        {
            auto count_ok = validate_arguments_count(t, 1);
            if (!count_ok)
                return std::unexpected(count_ok.error());

            return Help{};
        }
        else if (root == "exit")
        {
            auto count_ok = validate_arguments_count(t, 1);
            if (!count_ok)
                return std::unexpected(count_ok.error());

            return Exit{};
        }
        else if (root == "create-user")
        {
            return parse_create_user(t);
        }
        else if (root == "get-user")
        {
            return parse_user_get(t);
        }
        else if (root == "deposit")
        {
            return parse_wallet_deposit(t);
        }
        else if (root == "withdraw")
        {
            return parse_wallet_withdraw(t);
        }
        else if (root == "free-balance")
        {
            return parse_wallet_free_balance(t);
        }
        else if (root == "reserved-balance")
        {
            return parse_wallet_reserved_balance(t);
        }
        else if (root == "place-limit")
        {
            return parse_place_limit_order(t);
        }
        else if (root == "place-market")
        {
            return parse_execute_market_order(t);
        }
        else if (root == "cancel-order")
        {
            return parse_cancel_order(t);
        }
        else if (root == "register-market")
        {
            return parse_register_market(t);
        }
        else
        {
            return std::unexpected(ParseError{
                .stage = ParseStage::Parser,
                .code = ParseErrorCode::UnknownCommand,
                .message = "Unknown command",
                .column = t[0].index});
        }
    }

} // namespace vertex::cli
