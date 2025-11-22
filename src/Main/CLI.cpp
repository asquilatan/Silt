// Why did i make an entire CLI arg parser instead of using CLI11?
// Partly because i'm an idiot and forgot to look at CLI11 releases (it has an all-in-one cli11.hpp), but whatever i guess
// man_mining_for_diamonds.png
// -a

#include "CLI.hpp"
#include "Commands.hpp"
#include <iostream>
#include <string>


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

        // Validate against choices if choices are defined

        // if there are choices
        if (!choices.empty()) {
            bool valid_choice = false;
            // for every choice in choice
            for (const auto& choice : choices) {
                // if the next argument given is within the choice
                if (next_arg == choice) {
                    valid_choice = true;
                    break;
                }
            }

            // if not a valid choice
            if (!valid_choice) {
                // create a string of valid choices
                std::string valid_choices = "";
                for (size_t i = 0; i < choices.size(); ++i) {
                    // append choices to valid choices
                    if (i > 0) valid_choices += ", ";
                    valid_choices += choices[i];
                }
                // return an error string containing valid choices
                return "Error: Invalid value '" + next_arg + "' for argument " + dest_name +
                       ". Valid choices are: " + valid_choices;
            }
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
            // Validate against choices if choices are defined

            // if there are choices
            if (!choices.empty()) {
                bool valid_choice = false;
                // for every choice in choices
                for (const auto& choice : choices) {
                    // if the argument is a valid choice
                    if (curr_arg == choice) {
                        valid_choice = true;
                        break;
                    }
                }

                // if it's not a valid choice
                if (!valid_choice) {
                    // create a string of valid choices
                    std::string valid_choices = "";
                    // for every choice
                    for (size_t i = 0; i < choices.size(); ++i) {
                        // append choice to valid_choices
                        if (i > 0) valid_choices += ", ";
                        valid_choices += choices[i];
                    }
                    // return error string with valid choices
                    return "Error: Invalid value '" + curr_arg + "' for argument " + dest_name +
                           ". Valid choices are: " + valid_choices;
                }
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

    // Handle positional arguments for arguments that have choices and regular positional arguments
    // If an argument has choices and is required, check if we can assign positional arguments to it
    std::vector<std::string> remaining_positional_args = parsed_args.positional_args;
    parsed_args.positional_args.clear(); // Clear temporarily to reassign

    // First, handle arguments that have choices (they get priority)
    for (auto& argument : arguments) {
        // If this argument has choices and is required but not yet set
        if (!argument->choices.empty() && argument->required &&
            parsed_args.values.find(argument->dest_name) == parsed_args.values.end()) {

            // If we have any remaining positional args to assign
            if (!remaining_positional_args.empty()) {
                std::string pos_arg = remaining_positional_args[0];

                // Validate against choices
                bool valid_choice = false;
                for (const auto& choice : argument->choices) {
                    if (pos_arg == choice) {
                        valid_choice = true;
                        break;
                    }
                }

                if (!valid_choice) {
                    std::string valid_choices = "";
                    for (size_t i = 0; i < argument->choices.size(); ++i) {
                        if (i > 0) valid_choices += ", ";
                        valid_choices += argument->choices[i];
                    }
                    return "Error: Invalid value '" + pos_arg + "' for argument " + argument->dest_name + ". Valid choices are: " + valid_choices;
                }

                // Store the positional argument value to the argument name
                parsed_args.values[argument->dest_name] = pos_arg;
                remaining_positional_args.erase(remaining_positional_args.begin()); // Remove from remaining
            }
        }
    }

    // Then, handle remaining required arguments (without choices) in the order they appear
    for (auto& argument : arguments) {
        // If this argument is required, doesn't have choices and is not yet set
        if (argument->required && argument->choices.empty() &&
            parsed_args.values.find(argument->dest_name) == parsed_args.values.end()) {

            // If we have any remaining positional args to assign
            if (!remaining_positional_args.empty()) {
                std::string pos_arg = remaining_positional_args[0];

                // Store the positional argument value to the argument name
                parsed_args.values[argument->dest_name] = pos_arg;
                remaining_positional_args.erase(remaining_positional_args.begin()); // Remove from remaining
            }
        }
    }

    // Add any remaining positional args back to the positional_args vector
    for (const auto& pos_arg : remaining_positional_args) {
        parsed_args.positional_args.push_back(pos_arg);
    }

    // Check for required arguments that were not provided via flags or validated positional args
    for (auto& argument : arguments) {
        if (argument->required && parsed_args.values.find(argument->dest_name) == parsed_args.values.end() && parsed_args.multiple_values.find(argument->dest_name) == parsed_args.multiple_values.end()) {
            return "Error: Missing required argument: " + argument->dest_name;
        }
    }

    // Add default values for optional arguments not provided
    for (auto& argument : arguments) {
        if (!argument->required && parsed_args.values.find(argument->dest_name) == parsed_args.values.end() && parsed_args.multiple_values.find(argument->dest_name) == parsed_args.multiple_values.end()) {
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

// Set up parser
void setup_parser(Parser& parser) {

    // Create the "add" command
    auto add_cmd = std::make_unique<Command>(
        "add",
        "Add file contents to the index",
        cmd_add
    );

    // Create the "cat-file" command
    auto cat_file_cmd = std::make_unique<Command>(
        "cat-file",
        "Provide content of repository objects",
        cmd_cat_file
    );

    // Create the "check-ignore"
    auto check_ignore_cmd = std::make_unique<Command>(
        "check-ignore",
        "Check path(s) against ignore rules",
        cmd_check_ignore
    );

    auto checkout_cmd = std::make_unique<Command>(
        "checkout",
        "Switch branches or restore working tree files",
        cmd_checkout
    );


    auto commit_cmd = std::make_unique<Command>(
        "commit",
        "Record changes to the repository",
        cmd_commit
    );

    auto hash_object_cmd = std::make_unique<Command>(
        "hash-object",
        "Compute object ID and optionally creates a blob from a file",
        cmd_hash_object
    );

    auto init_cmd = std::make_unique<Command>(
        "init",
        "Create an empty Git repository or reinitialize an existing one",
        cmd_init
    );

    auto log_cmd = std::make_unique<Command>(
        "log",
        "Show commit logs",
        cmd_log
    );

    auto ls_files_cmd = std::make_unique<Command>(
        "ls-files",
        "List all the stage files",
        cmd_ls_files
    );

    auto ls_tree_cmd = std::make_unique<Command>(
        "ls-tree",
        "Recurse into sub-trees",
        cmd_ls_tree
    );

    auto rev_parse_cmd = std::make_unique<Command>(
        "rev-parse",
        "Parse revision (or other objects) identifiers",
        cmd_rev_parse
    );

    auto rm_cmd = std::make_unique<Command>(
        "rm",
        "Remove files from the working tree and from the index",
        cmd_rm
    );

    auto show_ref_cmd = std::make_unique<Command>(
        "show-ref",
        "List references in a local repository",
        cmd_show_ref
    );

    auto status_cmd = std::make_unique<Command>(
        "status",
        "Show the working tree status",
        cmd_status
    );

    auto tag_cmd = std::make_unique<Command>(
        "tag",
        "Create, list, delete or verify a tag object signed with GPG",
        cmd_tag
    );

    // Add arguments to the "add" command
    add_cmd->add_argument(std::make_unique<Argument>(
        "file",              // dest_name
        1,                   // nargs (takes 1 value)
        "Specify file to add",  // help_text
        false,               // required
        "."                  // default_value
    ));

    // add verbose to add
    add_cmd->add_argument(std::make_unique<Argument>(
        "verbose",           // dest_name
        0,                   // nargs (is a flag)
        "Be verbose",        // help_text
        false,               // required
        "false",             // default_value
        "v",                 // short_opt
        "verbose"            // long_opt
    ));

    // Add argument to init command for directory path
    init_cmd->add_argument(std::make_unique<Argument> (
        "directory",
        1,
        "Directory to initialize the repository in",
        false,
        "."
    ));

    // Define choices for cat-file type argument
    std::vector<std::string> type_choices = {"blob", "commit", "tag", "tree"};

    cat_file_cmd->add_argument(std::make_unique<Argument> (
        "type",
        1,
        "Specify the type [blob|commit|tag|tree]",
        true,
        type_choices,
        "",      // default_value
        "",      // short_opt
        "",      // long_opt
        true     // positional
    ));

    cat_file_cmd->add_argument(std::make_unique<Argument> (
        "object",
        1,
        "The object to display",
        true,
        "",      // default_value
        "",      // short_opt1
        "",      // long_opt
        true     // positional
    ));

    hash_object_cmd->add_argument(std::make_unique<Argument> (
        "type",
        1,
        "Specify the type",
        false,
        type_choices,
        "blob",
        "t",
        "type",
        false
    ));

    hash_object_cmd->add_argument(std::make_unique<Argument> (
        "write",
        0,
        "Actually write object into database",
        false,
        "",
        "w",
        "write",
        false
    ));

    hash_object_cmd->add_argument(std::make_unique<Argument> (
        "path",
        1,
        "Read object from <file>",
        true,    // required
        "",      // default_value - empty
        "",      // short_opt
        "",      // long_opt
        true     // positional
    ));

    log_cmd->add_argument(std::make_unique<Argument> (
        "commit",
        1,
        "Commit to start at.",
        false,
        "HEAD",
        "",
        "",
        false
    ));

    // Register the command with the parser
    parser.add_command(std::move(init_cmd));
    parser.add_command(std::move(add_cmd));
    parser.add_command(std::move(cat_file_cmd));
    parser.add_command(std::move(hash_object_cmd));
    parser.add_command(std::move(log_cmd));
}