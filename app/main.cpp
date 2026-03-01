#include <iostream>
#include <string>
#include "cli/cli_app.hpp"
#include "cli/printer.hpp"
#include "cli/parser.hpp"

int main()
{
    std::cout << "Vertex Matching Engine booted\n";

    vertex::cli::CliApp app;
    vertex::cli::Printer printer;

    std::string line;

    printer.print_help(std::cout);
    std::cout << '\n';

    while (std::getline(std::cin, line))
    {
        auto parsed = vertex::cli::parse_command(line);

        if (!parsed)
        {
            printer.print_parse_error(parsed.error(), std::cout);
            continue;
        }

        auto result = app.dispatch(parsed.value());

        printer.print_dispatch_result(result, std::cout);

        if (std::holds_alternative<vertex::cli::ExitRequested>(result))
            break;

        std::cout << '\n';
    }

    return 0;
}