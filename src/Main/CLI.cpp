#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>
#include <functional>

class Parser {
public:
    std::unordered_map<std::string, std::unique_ptr<Command>> command_registry;
    std::string description;

    Parser(const std::string& program_description) : description(program_description) {};
    
    void add_command(std::unique_ptr<Command> cmd) {
        // TODO: Implement this
    }

    std::optional<std::string> parse_and_dispatch(int argc, char* argv[], Repository* repo) {
        std::optional<std::string> res;
        // TODO: Implement this
        return res;
    }

    void print_help() {
        // TODO: Implement this
    }
};

class Command {
public:
    std::string name;
    std::string help_text;
    std::vector<std::unique_ptr<Argument>> arguments;
    std::function<void(const ParsedArgs&, Repository*)> handler_func;

    Command(
        const std::string& cmd_name, 
        const std::string& cmd_help_text, 
        std::function<void(const ParsedArgs&, Repository*)> handler
    ) : name(cmd_name), help_text(cmd_help_text), handler_func(handler) {};

    void add_argument(std::unique_ptr<Argument> arg) {
        // TODO: Implement this
    }

    std::optional<std::string> parse(int argc, char* argv[]) {
        std::optional<std::string> res;
        // TODO: Implement this
        return res;
    }

    void call_handler(const ParsedArgs& args, Repository* repo) {
        // TODO: Implement this
    }
};

class Argument {
public:
    std::string dest_name;
    std::string help_text;
    bool required;
    std::string default_value;

    bool is_flag;
    std::string short_opt;
    std::string long_opt;

    Argument(
        const std::string& dest,
        const std::string& help,
        bool req,
        const std::string& def_val,
        bool flag = false,
        const std::string& short_o = "",
        const std::string& long_o = ""
    ) : dest_name(dest), help_text(help), required(req), 
    default_value(def_val), is_flag(flag), 
    short_opt(short_o), long_opt(long_o) {};
    
    // TODO: Implement the methods
    virtual ~Argument() = default;
    virtual std::optional<std::string> parse_from_argv(int& current_argc, char**& current_argv, ParsedArgs* storage) = 0;
    bool matches_short(const std::string& token);
    bool matches_long(const std::string& token);
};

struct ParsedArgs {
    std::unordered_map<std::string, std::string> values;
    std::vector<std::string> positional_args;

    // TODO: Implement Methods
    std::string get(const std::string& key, const std::string& default_value = "");
    bool exists(const std::string& key);
};
