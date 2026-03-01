#pragma once

#include <string>

namespace vertex::cli
{
    enum class ParseStage
    {
        Tokenizer
    };

    enum class ParseErrorCode
    {
        EmptyLine,
        InvalidToken,
        UnterminatedQuote,
        UnexpectedCharacterAfterQuote,
    };

    struct ParseError
    {
        ParseStage stage;
        ParseErrorCode code;
        std::string message;
        size_t column;
    };
}
