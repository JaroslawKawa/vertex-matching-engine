#pragma once
#include <ostream>
#include "cli_app.hpp"
#include "parse_error.hpp"

namespace vertex::cli
{
    class Printer
    {
    private:
        
        std::string to_string(ParseStage stage);
        std::string to_string(ParseErrorCode error);
        std::string to_string(AppErrorCode error);
    public:
        void print_help(std::ostream &stream);
        void print_parse_error(const ParseError& error, std::ostream &stream);
        void print_dispatch_result(const DispatchResult& result, std::ostream &stream);
        Printer() = default;
    };
}
