// Why did i make an entire CLI arg parser instead of using CLI11?
// Partly because i'm an idiot and forgot to look at CLI11 releases (it has an all-in-one cli11.hpp), but whatever i guess
// man_mining_for_diamonds.png
// -a

#include "CLI.hpp"

std::optional<std::string> Argument::parse_from_argv(int& current_argc, char**& current_argv, ParsedArgs& storage) {
    // Consume the option token itself
    current_argc--;
    current_argv++;

    // Case 1: Flag argument (nargs == 0)
    if (nargs == 0) {
        storage.values[dest_name] = "true";
        return std::nullopt;
    }

    // Case 2: Single value argument (nargs == 1)
    if (nargs == 1) {
        if (current_argc < 1) {
            return "Error: Missing value for argument " + dest_name;
        }

        std::string next_arg = current_argv[0];
        if (next_arg.rfind("-", 0) == 0) {
            return "Error: Missing value for argument " + dest_name;
        }

        storage.values[dest_name] = next_arg;

        // Consume the value
        current_argc--;
        current_argv++;
        return std::nullopt;
    }

    // Case 3: Multiple value argument (nargs == -1)
    if (nargs == -1) {
        std::vector<std::string> collected_values;
        while (current_argc > 0) {
            std::string curr_arg = current_argv[0];
            if (curr_arg.rfind("-", 0) == 0) {
                break; // Stop at the next option
            }
            collected_values.push_back(curr_arg);
            current_argc--;
            current_argv++;
        }

        if (required && collected_values.empty()) {
            return "Error: Missing at least one value for argument " + dest_name;
        }

        // Store for both single-string access and multi-value access
        if (!collected_values.empty()) {
            storage.set_multiple(dest_name, collected_values);
            // Also combine into a single string for the 'values' map
            std::stringstream ss;
            for (size_t i = 0; i < collected_values.size(); ++i) {
                if (i != 0) ss << ",";
                ss << collected_values[i];
            }
            storage.values[dest_name] = ss.str();
        }
        return std::nullopt;
    }
    return "Error: Invalid 'nargs' configuration for " + dest_name;
}

// Helper function implementation
std::optional<std::string> parse_arguments(int argc, char* argv[], const std::vector<std::unique_ptr<Argument>>& arguments, ParsedArgs& parsed_args) {
    int i = 0;
    while (i < argc) {
        std::string arg = argv[i];

        bool matched = false;
        for (auto& argument : arguments) {
            if (argument->matches_short(arg) || argument->matches_long(arg)) {
                // Create a temporary view of the remaining arguments
                int remaining_argc = argc - i;
                char** remaining_argv = argv + i;

                auto result = argument->parse_from_argv(remaining_argc, remaining_argv, parsed_args);
                if (result.has_value()) {
                    return result; // Error occurred
                }

                // Update 'i' based on how many arguments were consumed
                i += (argc - i) - remaining_argc;
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
