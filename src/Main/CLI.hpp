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
class Repository; // Forward declaration

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

// Base argument class
class Argument {
public:
    // fields
    std::string dest_name;
    std::string help_text;
    bool required;
    int nargs; // 0 for flags, 1 for single value, -1 for multiple values
    std::string default_value;

    std::string short_opt;
    std::string long_opt;

    // constructor
    Argument(
        const std::string& dest,
        int num_args,
        const std::string& help,
        bool req,
        const std::string& def_val = "",
        const std::string& short_o = "",
        const std::string& long_o = ""
    ) : dest_name(dest), help_text(help), required(req), nargs(num_args),
    default_value(def_val), short_opt(short_o), long_opt(long_o) {};

    // Virtual destructor for the argument base class
    virtual ~Argument() = default;

    // Virtual function, both StringArgument and FlagArgument implement this.
    std::optional<std::string> parse_from_argv(int& current_argc, char**& current_argv, ParsedArgs& storage);

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
