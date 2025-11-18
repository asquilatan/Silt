
#include "CLI.hpp"
#include "Commands.hpp"
#include <iostream>

// Temporary Repository class definition for now
class Repository {
    // This will be replaced with actual Repository implementation later
};

int main(int argc, char* argv[]) {
    // Create parser with program description
    Parser parser("Silt - Version Control System");

    // Create the "add" command
    auto add_cmd = std::make_unique<Command>(
        "add",
        "Add file contents to the index",
        cmd_add
    );

    // Create the "cat-file" command
    auto cat_file_cmd = std::make_unique<Command>(
        "cat_file",
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
    add_cmd->add_argument(std::make_unique<StringArgument>(
        "file",              // dest_name
        "Specify file to add",  // help_text
        false,               // required
        "."                  // default_value
    ));

    // add verbose to add
    add_cmd->add_argument(std::make_unique<FlagArgument>(
        "verbose",           // dest_name
        "Be verbose",        // help_text
        "false",             // default_value
        "v",                 // short_opt
        "verbose"            // long_opt
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