#pragma once

#include <cstddef>
#include <string>

namespace vertex::cli
{
    enum class ParseStage
    {
        Tokenizer,
        Parser
    };

    enum class ParseErrorCode
    {
        EmptyLine,
        InvalidToken,
        UnterminatedQuote,
        UnexpectedCharacterAfterQuote,
        UnknownCommand,
        MissingArgument,
        TooManyArguments,
        InvalidName,
        InvalidNumber,
        InvalidId,
        InvalidAsset,
        InvalidMarket,
        InvalidSide,
    };

    struct ParseError
    {
        ParseStage stage;
        ParseErrorCode code;
        std::string message;
        std::size_t column;
    };
}
