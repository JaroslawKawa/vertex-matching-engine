#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "parse_error.hpp"

namespace vertex::cli
{
    struct Token
    {
        std::string_view text;
        std::size_t index{0};
    };

    std::expected<std::vector<Token>, ParseError> tokenize(std::string_view line);
}