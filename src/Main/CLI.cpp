// Why did i make an entire CLI arg parser instead of using CLI11?
// Partly because i'm an idiot and forgot to look at CLI11 releases (it has an all-in-one cli11.hpp), but whatever i guess
// man_mining_for_diamonds.png
// -a

#include "CLI.hpp"

// Helper function implementation
std::optional<std::string> parse_arguments(int argc, char* argv[], const std::vector<std::unique_ptr<Argument>>& arguments, ParsedArgs& parsed_args) {
    int i = 0;
    while (i < argc) {
        std::string arg = argv[i];

        bool matched = false;
        for (auto& argument : arguments) {
            if (argument->matches_short(arg) || argument->matches_long(arg)) {
                // Pass the remaining arguments to the argument's parser
                int temp_argc = argc - i;
                char** temp_argv = argv + i;

                auto result = argument->parse_from_argv(temp_argc, temp_argv, parsed_args);
                if (result.has_value()) {
                    return result; // Error occurred
                }

                // Calculate how many arguments were consumed and advance 'i'
                int args_consumed = (argc - i) - temp_argc;
                i += args_consumed; // This will be at least 1 for the option itself
                matched = true;
                break; // Exit inner loop after processing matched argument
            }
        }

        if (!matched) {
            // Check if the argument looks like a flag (starts with -) but doesn't match any defined arguments
            if (arg.length() > 1 && arg[0] == '-') {
                return "Error: Unknown argument '" + arg + "'";
            }
            // Only add to positional_args if it's not a flag-looking argument
            parsed_args.positional_args.push_back(arg);
            i++; // Advance to next command line argument if no match
        }
        // If there was a match, we've already incremented 'i' by the number of consumed args
    }

    // Check for required arguments
    for (auto& argument : arguments) {
        if (argument->required && parsed_args.values.find(argument->dest_name) == parsed_args.values.end()) {
            return "Error: Missing required argument: " + argument->dest_name;
        }
    }

    // Add default values for optional arguments not provided
    for (auto& argument : arguments) {
        if (!argument->required && parsed_args.values.find(argument->dest_name) == parsed_args.values.end()) {
            parsed_args.values[argument->dest_name] = argument->default_value;
        }
    }

    return std::nullopt; // Success
}

// Parser implementation
std::optional<std::string> Parser::parse_and_dispatch(int argc, char* argv[], Repository* repo) {
    // If there are no arguments
    if (argc < 2) {
        print_help();
        return "Error: No command provided";
    }

    std::string command_name = argv[1];

    // If the command is help
    if (command_name == "--help" || command_name == "-h") {
        print_help();
        return std::nullopt;
    }

    auto it = command_registry.find(command_name);
    // If the command doesn't exist
    if (it == command_registry.end()) {
        print_help();
        return "Error: Unknown command '" + command_name + "'";
    }

    // Adjust argc and argv to skip the program name and command name
    // e.g. `silt commit -m "feat: Add blobs"` -> `-m "feat: Add blobs"`
    int sub_argc = argc - 2;
    char** sub_argv = argv + 2;

    // Parse arguments and store them in a ParsedArgs instance
    ParsedArgs parsed_args;
    auto& command = it->second;

    // Handle help flag for the specific command
    if (sub_argc > 0) {
        std::string first_arg = sub_argv[0];
        if (first_arg == "--help" || first_arg == "-h") {
            command->print_help();
            return std::nullopt;
        }
    }

    // Use the centralized parsing function
    auto parse_result = parse_arguments(sub_argc, sub_argv, command->arguments, parsed_args);
    if (parse_result.has_value()) {
        return parse_result;
    }

    command->call_handler(parsed_args, repo);
    return std::nullopt; // Success
}

void Parser::print_help() {
    std::cout << description << std::endl;
    std::cout << "Available commands:" << std::endl;
    for (const auto& pair : command_registry) {
        std::cout << "  " << pair.first << " - " << pair.second->help_text << std::endl;
    }
}
