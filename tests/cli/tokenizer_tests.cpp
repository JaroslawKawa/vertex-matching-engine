#include <gtest/gtest.h>

#include "app/cli/tokenizer.hpp"

namespace
{
    using vertex::cli::ParseErrorCode;
    using vertex::cli::ParseStage;
    using vertex::cli::tokenize;
}

TEST(TokenizerTest, SplitsSimpleWhitespaceSeparatedTokens)
{
    const auto result = tokenize("deposit 1 USDT 100");

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 4u);
    EXPECT_EQ((*result)[0].text, "deposit");
    EXPECT_EQ((*result)[1].text, "1");
    EXPECT_EQ((*result)[2].text, "USDT");
    EXPECT_EQ((*result)[3].text, "100");
}

TEST(TokenizerTest, SupportsQuotedTokenWithSpaces)
{
    const auto result = tokenize("create-user \"Alice Bob\"");

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 2u);
    EXPECT_EQ((*result)[0].text, "create-user");
    EXPECT_EQ((*result)[1].text, "Alice Bob");
}

TEST(TokenizerTest, ReturnsUnterminatedQuoteError)
{
    const auto result = tokenize("create-user \"Alice Bob");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().stage, ParseStage::Tokenizer);
    EXPECT_EQ(result.error().code, ParseErrorCode::UnterminatedQuote);
}

TEST(TokenizerTest, ReturnsUnexpectedCharacterAfterQuoteError)
{
    const auto result = tokenize("create-user \"Alice\"x");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().stage, ParseStage::Tokenizer);
    EXPECT_EQ(result.error().code, ParseErrorCode::UnexpectedCharacterAfterQuote);
}

TEST(TokenizerTest, ReturnsEmptyLineForWhitespaceOnlyInput)
{
    const auto result = tokenize("   \t  ");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().stage, ParseStage::Tokenizer);
    EXPECT_EQ(result.error().code, ParseErrorCode::EmptyLine);
}
