
#include "CLI.hpp"
#include <iostream>

// Temporary Repository class definition for now
class Repository {
    // This will be replaced with actual Repository implementation later
};

// Handler function for the "add" command, this WILL be put in Commands.hpp some commits down the line, i just wanted to test if it works
void cmd_add(const ParsedArgs& args, Repository* repo) {
    std::cout << "Executing 'add' command" << std::endl;

    // Get the files from positional arguments (files to add)
    for (const auto& file : args.positional_args) {
        if (file != "add") {  // Skip the command name itself
            std::cout << "Adding file: " << file << std::endl;
        }
    }

    // Check for file argument
    if (args.exists("file") && !args.get("file").empty()) {
        std::cout << "Adding file (via --file): " << args.get("file") << std::endl;
    }

    // Check for verbose flag - flag is set when its value is "true"
    if (args.exists("verbose") && args.get("verbose") == "true") {
        std::cout << "Verbose mode enabled" << std::endl;
    }

    // Check for update flag - flag is set when its value is "true"
    if (args.exists("update") && args.get("update") == "true") {
        std::cout << "Update mode enabled" << std::endl;
    }
}

int main(int argc, char* argv[]) {
    // Create parser with program description
    Parser parser("Silt - Version Control System");

    // Create the "add" command
    auto add_cmd = std::make_unique<Command>(
        "add",
        "Add file contents to the index",
        cmd_add
    );

    // Add arguments to the "add" command
    add_cmd->add_argument(std::make_unique<StringArgument>(
        "file",              // dest_name
        "Specify file to add",  // help_text
        false,               // required
        "",                  // default_value
        false,               // is_flag (not a flag argument)
        "f",                 // short_opt
        "file"               // long_opt
    ));

    add_cmd->add_argument(std::make_unique<FlagArgument>(
        "verbose",           // dest_name
        "Enable verbose output",  // help_text
        "",                  // default_value
        "v",                 // short_opt
        "verbose"            // long_opt
    ));

    add_cmd->add_argument(std::make_unique<FlagArgument>(
        "update",            // dest_name
        "Update only the specified files",  // help_text
        "",                  // default_value
        "u",                 // short_opt
        "update"             // long_opt
    ));

    // Register the command with the parser
    parser.add_command(std::move(add_cmd));

    // Create a dummy repository instance for now
    Repository repo;

    // Parse arguments and dispatch to the appropriate command
    auto result = parser.parse_and_dispatch(argc, argv, &repo);

    // If there was an error, print it
    if (result.has_value()) {
        std::cerr << result.value() << std::endl;
        return 1;
    }

    return 0;
}