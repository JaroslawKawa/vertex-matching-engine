#include "tokenizer.hpp"
#include <cctype>
#include <cstddef>
namespace vertex::cli
{
    std::expected<std::vector<Token>, ParseError> tokenize(std::string_view line)
    {
        if (line.empty())
            return std::unexpected(ParseError{
                .stage = ParseStage::Tokenizer,
                .code = ParseErrorCode::EmptyLine,
                .message = "Empty line",
                .column = 0});

        std::vector<Token> result;
        std::size_t pos = 0;

        while (pos < line.size())
        {
            while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos])))
                ++pos;

            if (pos >= line.size())
                break;

            if (line[pos] == '\"')
            {
                const std::size_t quote_pos = pos;
                ++pos;

                const std::size_t start = pos;

                while (pos < line.size() && !(line[pos] == '\"'))
                    ++pos;

                if (pos == line.size())
                    return std::unexpected(ParseError{
                        .stage = ParseStage::Tokenizer,
                        .code = ParseErrorCode::UnterminatedQuote,
                        .message = "Unterminated quote",
                        .column = quote_pos});

                result.push_back(Token{
                    .text = line.substr(start, pos - start),
                    .index = quote_pos});

                ++pos;

                if (pos < line.size() && !std::isspace(static_cast<unsigned char>(line[pos])))
                    return std::unexpected(ParseError{
                        .stage = ParseStage::Tokenizer,
                        .code = ParseErrorCode::UnexpectedCharacterAfterQuote,
                        .message = "Character after the end of a quote must be whitespace",
                        .column = pos});
            }
            else
            {
                const std::size_t start = pos;

                while (pos < line.size() && !std::isspace(static_cast<unsigned char>(line[pos])))
                    ++pos;
                result.push_back(Token{
                    .text = line.substr(start, pos - start),
                    .index = start});
            }
        }

        if (result.empty())
        {
            return std::unexpected(ParseError{
                .stage = ParseStage::Tokenizer,
                .code = ParseErrorCode::EmptyLine,
                .message = "Empty line",
                .column = 0});
        }

        return result;
    }
}
