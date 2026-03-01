#pragma once
#include <expected>
#include <string_view>
#include "command.hpp"
#include "parse_error.hpp"

namespace vertex::cli
{

    std::expected<Command,ParseError> parse_command(std::string_view line);


} // namespace vertex::cli
