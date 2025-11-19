#include "Commands.hpp"
#include "CLI.hpp"
#include <iostream>
#include <vector>
#include <string>
#include "Repository.hpp" // Include the header for Repository
#include <filesystem>

// Implementation of cmd_add to handle multiple paths from positional_args
void cmd_add(const ParsedArgs& args, Repository* repo) {
    // Get the file argument if provided specifically with -f flag
    std::string specific_file = args.get("file");

    // Get paths from positional arguments (for multiple files passed without flags)
    std::vector<std::string> paths = args.positional_args;

    // If positional arguments are empty and a specific file was given (which might be the default)
    if (paths.empty() && !specific_file.empty()) {
        paths.push_back(specific_file);
    }

    // If no paths were provided at all (neither positional nor via options)
    if (paths.empty()) {
        paths.push_back(".");
    }

    // Process each path
    for (const auto& path : paths) {
        std::cout << "Would add: " << path << std::endl;
    }

    // if the verbose flag is on
    if (args.exists("verbose") && args.get("verbose") == "true") {
        std::cout << "Verbose mode enabled." << std::endl;
    }

}

// Implementation of cmd_check_ignore to handle multiple paths
void cmd_check_ignore(const ParsedArgs& args, Repository* repo) {
    // Get paths from positional arguments (for multiple files)
    std::vector<std::string> paths = args.positional_args;

    // Process each path to check if it's ignored
    for (const auto& path : paths) {
        // In a real implementation, this would check the ignore patterns
        // For now, just print the path being checked
        std::cout << "Checking ignore status for: " << path << std::endl;
        // Example: if the file matches a pattern in .gitignore, print it
        // For now, assume no files are ignored
        std::cout << path << " is not ignored" << std::endl;
    }
}

// Implementation of cmd_rm to handle multiple paths
void cmd_rm(const ParsedArgs& args, Repository* repo) {
    // Get paths from positional arguments (for multiple files)
    std::vector<std::string> paths = args.positional_args;

    // Process each path for removal
    for (const auto& path : paths) {
        // In a real implementation, this would call the actual rm logic
        // For now, just print the path being removed
        std::cout << "Would remove: " << path << std::endl;
    }
}

// Placeholders for other commands until they're implemented
void cmd_cat_file(const ParsedArgs& args, Repository* repo) {
    std::cout << "cat-file command not yet implemented" << std::endl;
}

void cmd_checkout(const ParsedArgs& args, Repository* repo) {
    std::cout << "checkout command not yet implemented" << std::endl;
}

void cmd_commit(const ParsedArgs& args, Repository* repo) {
    std::cout << "commit command not yet implemented" << std::endl;
}

void cmd_hash_object(const ParsedArgs& args, Repository* repo) {
    std::cout << "hash-object command not yet implemented" << std::endl;
}

void cmd_init(const ParsedArgs& args, Repository* repo) {
    std::filesystem::path init_path;

    // Check for directory argument (could be passed as --directory mydir)
    std::string directory_arg = args.get("directory");

    // Use the first positional argument as the path if provided,
    // otherwise use the directory argument, or default to current directory
    if (!args.positional_args.empty()) {
        init_path = args.positional_args[0];
    } else if (!directory_arg.empty() && directory_arg != ".") {
        init_path = directory_arg;
    } else {
        init_path = "."; // Default to current directory if no path provided
    }

    try {
        Repository new_repo = repo_create(init_path); // Call repo_create which returns a Repository object
        std::cout << "Initialized empty Silt repository in " << new_repo.gitdir << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error initializing repository: " << e.what() << std::endl;
    }
}

void cmd_log(const ParsedArgs& args, Repository* repo) {
    std::cout << "log command not yet implemented" << std::endl;
}

void cmd_ls_files(const ParsedArgs& args, Repository* repo) {
    std::cout << "ls-files command not yet implemented" << std::endl;
}

void cmd_ls_tree(const ParsedArgs& args, Repository* repo) {
    std::cout << "ls-tree command not yet implemented" << std::endl;
}

void cmd_rev_parse(const ParsedArgs& args, Repository* repo) {
    std::cout << "rev-parse command not yet implemented" << std::endl;
}

void cmd_show_ref(const ParsedArgs& args, Repository* repo) {
    std::cout << "show-ref command not yet implemented" << std::endl;
}

void cmd_status(const ParsedArgs& args, Repository* repo) {
    std::cout << "status command not yet implemented" << std::endl;
}

void cmd_tag(const ParsedArgs& args, Repository* repo) {
    std::cout << "tag command not yet implemented" << std::endl;
}