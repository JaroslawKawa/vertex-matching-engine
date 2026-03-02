#include <gtest/gtest.h>

#include <sstream>

#include "app/cli/printer.hpp"

namespace
{
    using vertex::cli::AppError;
    using vertex::cli::AppErrorCode;
    using vertex::cli::DispatchResult;
    using vertex::cli::ExitRequested;
    using vertex::cli::HelpRequested;
    using vertex::cli::ParseError;
    using vertex::cli::ParseErrorCode;
    using vertex::cli::ParseStage;
    using vertex::cli::Printer;
    using vertex::cli::UserCreated;
}

TEST(PrinterTest, PrintHelpContainsCoreCommands)
{
    Printer printer;
    std::ostringstream out;

    printer.print_help(out);
    const auto text = out.str();

    EXPECT_NE(text.find("Vertex Matching Engine CLI"), std::string::npos);
    EXPECT_NE(text.find("create-user <name>"), std::string::npos);
    EXPECT_NE(text.find("place-limit <user_id> <base>/<quote> <buy|sell> <price> <quantity>"), std::string::npos);
}

TEST(PrinterTest, PrintParseErrorShowsStageCodeColumnAndMessage)
{
    Printer printer;
    std::ostringstream out;

    const ParseError error{
        .stage = ParseStage::Parser,
        .code = ParseErrorCode::InvalidSide,
        .message = "Side must be buy or sell",
        .column = 12};

    printer.print_parse_error(error, out);
    const auto text = out.str();

    EXPECT_NE(text.find("[ERROR] [Parser] [InvalidSide]"), std::string::npos);
    EXPECT_NE(text.find("At postion 12"), std::string::npos);
    EXPECT_NE(text.find("Side must be buy or sell"), std::string::npos);
}

TEST(PrinterTest, PrintDispatchResultForExitRequested)
{
    Printer printer;
    std::ostringstream out;

    const DispatchResult result = ExitRequested{};
    printer.print_dispatch_result(result, out);

    EXPECT_EQ(out.str(), "[INFO] Exit requested");
}

TEST(PrinterTest, PrintDispatchResultForHelpRequested)
{
    Printer printer;
    std::ostringstream out;

    const DispatchResult result = HelpRequested{};
    printer.print_dispatch_result(result, out);

    EXPECT_NE(out.str().find("Commands:"), std::string::npos);
}

TEST(PrinterTest, PrintDispatchResultForUserCreated)
{
    Printer printer;
    std::ostringstream out;

    const DispatchResult result = UserCreated{.user_id = 42, .name = "Alice"};
    printer.print_dispatch_result(result, out);

    EXPECT_EQ(out.str(), "[SUCCESS] User created: id=42 name=Alice");
}

TEST(PrinterTest, PrintDispatchResultForAppError)
{
    Printer printer;
    std::ostringstream out;

    const DispatchResult result = AppError{.code = AppErrorCode::UserNotFound, .message = "User not found"};
    printer.print_dispatch_result(result, out);

    EXPECT_EQ(out.str(), "[ERROR][UserNotFound] User not found");
}
