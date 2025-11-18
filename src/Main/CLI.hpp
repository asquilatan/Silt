#pragma once

#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>
#include <functional>
#include <sstream>

// Forward declaration for ParsedArgs to resolve circular dependency with Argument
class ParsedArgs;
class Repository; // Forward declaration

// Base argument class
class Argument {
public:
    // fields
    std::string dest_name;
    std::string help_text;
    bool required;
    std::string default_value;

    bool is_flag;
    std::string short_opt;
    std::string long_opt;

    // constructor
    Argument(
        const std::string& dest,
        const std::string& help,
        bool req,
        const std::string& def_val = "",
        bool flag = false,
        const std::string& short_o = "",
        const std::string& long_o = ""
    ) : dest_name(dest), help_text(help), required(req),
    default_value(def_val), is_flag(flag),
    short_opt(short_o), long_opt(long_o) {};

    // Virtual destructor for the argument base class
    virtual ~Argument() = default;

    // Virtual function, both StringArgument and FlagArgument implement this.
    virtual std::optional<std::string> parse_from_argv(int& current_argc, char**& current_argv, ParsedArgs& storage) = 0;

    // Checks if the token is equal to the short option
    bool matches_short(const std::string& token) {
        if (short_opt.empty())
            return false;
        return token == "-" + short_opt;
    }

    // Checks if the token is equal to the long option
    bool matches_long(const std::string& token) {
        if (long_opt.empty())
            return false;
        return token == "--" + long_opt;
    }
};

// Parsed arguments class (was supposed to be a struct but whatever)
class ParsedArgs {
public:
    // fields
    std::unordered_map<std::string, std::string> values;
    std::unordered_map<std::string, std::vector<std::string>> multiple_values;
    std::vector<std::string> positional_args;

    // Gets the value using the key
    std::string get(const std::string& key, const std::string& default_value = "") const {
        auto it = values.find(key);
        if (it != values.end()) {
            return it->second;
        }
        return default_value;
    }

    // Gets multiple values using the key (for nargs arguments)
    std::vector<std::string> get_multiple(const std::string& key, const std::vector<std::string>& default_values = {}) const {
        auto it = multiple_values.find(key);
        if (it != multiple_values.end()) {
            return it->second;
        }
        return default_values;
    }

    // Sets multiple values
    void set_multiple(const std::string& key, const std::vector<std::string>& values) {
        multiple_values[key] = values;
    }

    // Checks if a key exists in values
    bool exists(const std::string& key) const {
        return values.find(key) != values.end();
    }

    // Checks if a multiple values key exists
    bool exists_multiple(const std::string& key) const {
        return multiple_values.find(key) != multiple_values.end();
    }
};

// Concrete argument classes

// String argument, inherits from Argument
class StringArgument : public Argument {
public:
    // Constructor for String Argument
    StringArgument(
        const std::string& dest,
        const std::string& help,
        bool req,
        const std::string& def_val = "",
        bool flag = false,
        const std::string& short_o = "",
        const std::string& long_o = ""
    ) : Argument(dest, help, req, def_val, flag, short_o, long_o) {}

    // Implementation of parse_from_argv
    std::optional<std::string> parse_from_argv(int& current_argc, char**& current_argv, ParsedArgs& storage) override {
        // If there are no given requirements (if the count is zero or negative)
        if (current_argc <= 0) {
            // If it is required
            if (required) {
                return "Error: Missing required argument for " + dest_name;
            }
            // If not required, it will get the default value later
            return std::nullopt;
        }

        // For flag arguments, they don't take a value, so just set to "true"
        if (is_flag) {
            storage.values[dest_name] = "true";
            return std::nullopt;
        }

        // For non-flag arguments, look for the next argument as the value
        if (current_argc > 1) {
            std::string next_arg = current_argv[1];
            // Check if the next argument is another option. If so, this one is missing its value.
            if (next_arg.rfind("-", 0) == 0) {
                return "Error: Missing value for argument " + dest_name;
            }

            // If it was provided a value, replace values.dest_name with the next argument
            storage.values[dest_name] = next_arg;

            // Consume the option and its value
            current_argc -= 2;
            current_argv += 2;
            return std::nullopt;

        // If there is only ONE argument left (no next arg to use as value)
        } else {
            // This is the last token, but it's an option that needs a value.
            return "Error: Missing value for argument " + dest_name;
        }
    }
};

// N-argument (nargs) class for handling multiple values
class NargsArgument : public Argument {
public:
    // Constructor for Nargs Argument
    NargsArgument(
        const std::string& dest,
        const std::string& help,
        bool req,
        const std::string& def_val = "",
        bool flag = false,
        const std::string& short_o = "",
        const std::string& long_o = ""
    ) : Argument(dest, help, req, def_val, flag, short_o, long_o) {}

    // Implementation of parse_from_argv for collecting multiple arguments
    std::optional<std::string> parse_from_argv(int& current_argc, char**& current_argv, ParsedArgs& storage) override {
        // Skip the option itself first (consume the flag)
        current_argc--;
        current_argv++;

        // Collect all following non-option arguments until we hit another option or run out
        std::vector<std::string> values;

        while (current_argc > 0) {
            std::string curr_arg = current_argv[0];
            // If the current argument is an option, stop collecting
            if (curr_arg.rfind("-", 0) == 0) {
                break;
            }

            values.push_back(curr_arg);
            current_argc--;
            current_argv++;
        }

        // Store the collected values as a comma-separated string
        std::string combined_values;
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) combined_values += ",";
            combined_values += values[i];
        }

        storage.values[dest_name] = combined_values;
        // Also store it in multiple_values map for easier access
        storage.set_multiple(dest_name, values);
        return std::nullopt;
    }
};

// Flag argument, inherits from Argument
class FlagArgument : public Argument {
public:
    // Constructor for Flag Argument
    FlagArgument(
        const std::string& dest,
        const std::string& help,
        const std::string& def_val = "",
        const std::string& short_o = "",
        const std::string& long_o = ""
    ) : Argument(dest, help, false, def_val, true, short_o, long_o) {}

    // Implementation of parse_from_argv
    std::optional<std::string> parse_from_argv(int& current_argc, char**& current_argv, ParsedArgs& storage) override {
        storage.values[dest_name] = "true";
        // Consume the current argument (the flag itself)
        current_argc--;
        current_argv++;
        return std::nullopt;
    }
};

// Helper function declaration
std::optional<std::string> parse_arguments(int argc, char* argv[], const std::vector<std::unique_ptr<Argument>>& arguments, ParsedArgs& parsed_args);

// CLI classes
class Command {
public:
    // fields
    std::string name;
    std::string help_text;
    std::vector<std::unique_ptr<Argument>> arguments;
    std::function<void(const ParsedArgs&, Repository*)> handler_func;

    // Constructor for Command
    Command(
        const std::string& cmd_name,
        const std::string& cmd_help_text,
        std::function<void(const ParsedArgs&, Repository*)> handler
    ) : name(cmd_name), help_text(cmd_help_text), handler_func(handler) {};

    // Add an arg to the arguments vector
    void add_argument(std::unique_ptr<Argument> arg) {
        arguments.push_back(std::move(arg));
    }

    // Executes the function associated with the command after it has been parsed
    void call_handler(const ParsedArgs& args, Repository* repo) {
        if (handler_func) {
            handler_func(args, repo);
        }
    }

    // Creates a simple help message, triggered when -h or --help is called
    void print_help() {
        std::cout << name << " - " << help_text << std::endl;
        std::cout << "Options:" << std::endl;
        // For every argument in arguments
        for (auto& arg : arguments) {
            std::string opt_str = "";
            // If there's a short arg/option
            if (!arg->short_opt.empty()) {
                opt_str += "-" + arg->short_opt;
            }
            // If there's a long arg/option
            if (!arg->long_opt.empty()) {
                if (!opt_str.empty()) opt_str += ", ";
                opt_str += "--" + arg->long_opt;
            }
            std::cout << "  " << opt_str << " - " << arg->help_text << std::endl;
        }
    }
};

// CLI Parser class
class Parser {
public:
    // fields
    std::unordered_map<std::string, std::unique_ptr<Command>> command_registry;
    std::string description;

    // Constructor for Parser
    Parser(const std::string& program_description) : description(program_description) {};

    // Add a command to the command_registry
    void add_command(std::unique_ptr<Command> cmd) {
        command_registry[cmd->name] = std::move(cmd);
    }

    // Parses the command-line arguments and dispatches to the appropriate command handler.
    std::optional<std::string> parse_and_dispatch(int argc, char* argv[], Repository* repo);

    void print_help();
};
