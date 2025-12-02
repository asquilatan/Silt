#include "Commands.hpp"
#include "CLI.hpp"
#include <iostream>
#include <vector>
#include <string> // Include the header for GitObject and related functions
#include "Objects.hpp"
#include "Repository.hpp" // Include the header for Repository
#include <filesystem>
#include <set>

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
    // find repo with repo_find()
    // call cat_file, passing repo, object in args, and fmt=args.type but encoded
    std::string type = args.get("type");
    std::string object = args.get("object");

    // If the repository is not found, attempt to find it from the current directory
    if (!repo) {
        std::optional<Repository> found_repo = repo_find(std::filesystem::current_path(), true);
        if (found_repo.has_value()) {
            repo = &found_repo.value();
        } else {
            std::cerr << "Error: Not a Git repository." << std::endl;
            return;
        }
    }

    cat_file(repo, object, type);
}

void cat_file(Repository* repo, std::string object, std::string fmt) {
    // object_read, pass in repo and the return of object_find(repo, obj, fmt=fmt)
    std::optional<std::unique_ptr<GitObject>> obj_opt = object_read(repo, const_cast<char*>(object_find(repo, object, fmt, true).c_str())); // object_find is defined in Objects.cpp
    // if object has a value
    if (obj_opt.has_value()) {
        std::unique_ptr<GitObject> obj = std::move(obj_opt.value());
        std::cout << obj->serialize();
    } else {
        std::cerr << "Error: Object " << object << " not found." << std::endl;
    }
}

/*
 * Problem: cmd_checkout (Implementation)
 * ---------------------------------------------------------------------------
 * Description:
 *   Implement the bridge function for the "checkout" command in Silt. This
 *   function should parse the provided arguments, locate the specified commit
 *   or tree object, and extract its contents to the specified directory path.
 *   The target directory must be empty to prevent data loss.
 *
 * Input:
 *   - args: ParsedArgs containing:
 *       - "commit": A commit SHA, tree SHA, branch name, tag, or "HEAD"
 *       - "path": The target directory path to checkout into (must be empty)
 *   - repo: Pointer to the current Repository object
 *
 * Output:
 *   - Files and directories from the commit/tree are created in the target path
 *   - Prints error messages to stderr if checkout fails
 *
 * Algorithm:
 *   1. Find the repository if not provided
 *   2. Resolve the commit reference using object_find()
 *   3. Read the object - if it's a commit, extract its tree reference
 *   4. Verify target path is empty or create it
 *   5. Call tree_checkout() to recursively extract contents
 *
 * Example:
 *   Input:  args = { commit: "HEAD", path: "./output" }, repo = <valid repo>
 *   Output: Contents of HEAD commit extracted to ./output/
 *
 * Constraints:
 *   - Target path must exist and be an empty directory, OR not exist (will be created)
 *   - If commit points to a commit object, extract its tree
 *   - If commit points directly to a tree, use that tree
 */
void cmd_checkout(const ParsedArgs& args, Repository* repo) {
    // TODO: Implement this function
    std::cout << "checkout command not yet implemented" << std::endl;
}

/*
 * Problem: tree_checkout (Implementation)
 * ---------------------------------------------------------------------------
 * Description:
 *   Recursively extract the contents of a Git tree object to a filesystem path.
 *   For each entry in the tree:
 *   - If it's a blob (file), read the blob and write its contents to the path
 *   - If it's a tree (directory), create the directory and recurse into it
 *
 * Input:
 *   - repo: Pointer to the Repository for reading objects
 *   - tree: Reference to the GitTree object to extract
 *   - target_path: Filesystem path where contents should be written
 *
 * Output:
 *   - Files and directories are created in target_path matching the tree structure
 *   - Blob contents are written as binary files
 *   - Directories are created for tree entries before recursing
 *
 * Algorithm:
 *   1. Iterate over each leaf in tree.get_leaves()
 *   2. Construct destination path: target_path / leaf.path
 *   3. Read the object using leaf.sha
 *   4. If object is a tree: create directory, recurse with subtree
 *   5. If object is a blob: write blob data to file
 *
 * Example:
 *   Input:  tree with entries [ {100644, "hello.txt", sha1}, {040000, "src", sha2} ]
 *           target_path = "/tmp/checkout"
 *   Output: Creates /tmp/checkout/hello.txt (with blob content)
 *                   /tmp/checkout/src/ (directory)
 *                   /tmp/checkout/src/... (contents of subtree sha2)
 *
 * Constraints:
 *   - target_path should already exist
 *   - Symlinks (mode 120000) are optional; can treat as regular files
 *   - Binary files should be written correctly (no text mode conversion)
 */
void tree_checkout(Repository* repo, const GitTree& tree, const std::filesystem::path& target_path) {
    // TODO: Implement this function
}

void cmd_commit(const ParsedArgs& args, Repository* repo) {
    std::cout << "commit command not yet implemented" << std::endl;
}

void cmd_hash_object(const ParsedArgs& args, Repository* repo) {
    // Get the file path from arguments
    std::string path = args.get("path");

    // Get the type, defaulting to "blob" if not provided
    std::string type = args.get("type", "blob");

    // Check if we should write to the repository
    bool write = args.exists("write");

    if (path.empty()) {
        std::cerr << "Error: Path argument is required." << std::endl;
        return;
    }

    // Read the file content as binary
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << path << std::endl;
        return;
    }

    // Read the entire file content into a string
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    // Call object_hash to compute the SHA1 hash
    std::string sha = object_hash(content, type, write ? repo : nullptr);

    // Print the SHA
    std::cout << sha << std::endl;
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
    std::cout << "digraph siltlog{" << std::endl;
    std::cout << "  node[shape=rect]" << std::endl;
    std::set<std::string> seen;
    log_graphviz(repo, object_find(repo, args.get("commit"), "", true), seen);
    std::cout << "}" << std::endl;
}

std::string log_graphviz(Repository* repo, std::string sha, std::set<std::string> seen) {
    // if sha was already seen
    if (seen.count(sha)) {
        return sha; // return if already processed
    }
    // otherwise, add sha to seen
    seen.insert(sha);

    // commit = object_read(repo, sha)
    char* sha_ptr = const_cast<char*>(sha.c_str());
    std::optional<std::unique_ptr<GitObject>> obj_opt = object_read(repo, sha_ptr);

    // if object does not have a value (nullopt)
    if (!obj_opt.has_value()) {
        std::cerr << "Error: Object " << sha << " not found." << std::endl;
        return sha;
    }

    std::unique_ptr<GitObject> obj = std::move(obj_opt.value());

    // check if the object format is commit
    if (obj->get_fmt() != "commit") {
        std::cerr << "Error: Object " << sha << " is not a commit." << std::endl;
        return sha;
    }

    // Cast to GitCommit to access kvlm
    GitCommit* commit = dynamic_cast<GitCommit*>(obj.get());
    if (!commit) {
        std::cerr << "Error: Could not cast object to commit." << std::endl;
        return sha;
    }

    // Since kvlm is private, we need to use the serialize method and re-parse it
    // or add a getter method. For now I'll use the serialize method and re-parse.
    std::string serialized_data = commit->serialize();
    KVLM kvlm = kvlm_parse(serialized_data);

    // message = commit.kvlm[""].decode("utf8").strip()
    std::string message;
    auto it = kvlm.find("");
    if (it != kvlm.end()) {
        message = std::get<std::string>(it->second);
    } else {
        message = "No message";
    }

    // Replace \ with \\ to escape backslashes
    size_t pos = 0;
    while ((pos = message.find('\\', pos)) != std::string::npos) {
        message.replace(pos, 1, "\\\\");
        pos += 2; // Move past the new "\\""
    }

    // Replace quotes with escaped quotes
    pos = 0;
    while ((pos = message.find('"', pos)) != std::string::npos) {
        message.replace(pos, 1, "\\\"");
        pos += 2; // Move past the new \"
    }

    // If newline is in message, keep only the first line
    size_t newline_pos = message.find('\n');
    if (newline_pos != std::string::npos) {
        message = message.substr(0, newline_pos);
    }

    // cout "   c_" << sha << "[label=\"" << sha[0:7] << ":" << message << "\"]" << endl;
    std::string short_sha = sha.substr(0, 7);
    std::cout << "   c_" << sha << "[label=\"" << short_sha << ":" << message << "\"]" << std::endl;

    // if parent is not in kvlm keys (if it's the initial commit)
    auto parent_it = kvlm.find("parent");
    if (parent_it == kvlm.end()) {
        return sha; // Initial commit with no parents
    }

    // parents = commit.kvlm["parent"]
    std::vector<std::string> parents;
    if (std::holds_alternative<std::string>(parent_it->second)) {
        // if the type of parents is not list (no multi parents), normalize by putting in vector
        parents.push_back(std::get<std::string>(parent_it->second));
    } else {
        // if it's already a vector
        parents = std::get<std::vector<std::string>>(parent_it->second);
    }

    // for every parent in parents
    for (const std::string& parent : parents) {
        // p = decoded version of parent
        std::string p = parent; // already a string in our implementation
        // cout << "   c_" << sha << " -> c_" << p << ";" << endl;
        std::cout << "   c_" << sha << " -> c_" << p << ";" << std::endl;

        // log_graphviz(repo, p, seen)
        log_graphviz(repo, p, seen);
    }

    return sha;
}

void cmd_ls_files(const ParsedArgs& args, Repository* repo) {
    std::cout << "ls-files command not yet implemented" << std::endl;
}

void cmd_ls_tree(const ParsedArgs& args, Repository* repo) {
    // get the tree-ish reference
    std::string tree_ref = args.get("tree");
    std::string recursive_str = args.get("recursive");
    bool recursive = (recursive_str == "true");

    // find the repository if not provided
    std::optional<Repository> found_repo;
    if (!repo) {
        found_repo = repo_find(std::filesystem::current_path(), true);
        // if found_repo is not None
        if (found_repo.has_value()) {
            repo = &found_repo.value();
        } else {
            std::cerr << "Error: Not a Git repository." << std::endl;
            return;
        }
    }

    // resolve the tree-ish reference to a tree SHA
    // first try to resolve as a tree
    std::string tree_sha = object_find(repo, tree_ref, "tree", true);

    // if that didn't work, try to resolve as a commit and get its tree
    if (tree_sha.empty()) {
        // resolve the tree-ish reference to a commit SHA
        tree_sha = object_find(repo, tree_ref, "commit", true);
        // if we found a commit
        if (!tree_sha.empty()) {
            // read the commit object to get its tree SHA
            std::optional<std::unique_ptr<GitObject>> obj_opt = object_read(repo, const_cast<char*>(tree_sha.c_str()));
            // if obj is not None
            if (obj_opt.has_value()) {
                // cast obj to GitCommit
                std::unique_ptr<GitObject> obj = std::move(obj_opt.value());
                // if obj is a commit
                if (obj->get_fmt() == "commit") {
                    GitCommit* commit = dynamic_cast<GitCommit*>(obj.get());
                    // if commit is not None
                    if (commit) {
                        // get the tree SHA from the commit's kvlm
                        std::string serialized = commit->serialize();
                        // parse kvlm to get tree
                        KVLM kvlm = kvlm_parse(serialized);
                        // find "tree" in kvlm
                        auto tree_it = kvlm.find("tree");
                        // if the tree key exists and is a string
                        if (tree_it != kvlm.end() && std::holds_alternative<std::string>(tree_it->second)) {
                            // assign tree_sha
                            tree_sha = std::get<std::string>(tree_it->second);
                        } else {
                            std::cerr << "Error: Could not find tree in commit." << std::endl;
                            return;
                        }
                    }
                }
            }
        }
    }

    if (tree_sha.empty()) {
        std::cerr << "Error: Could not resolve tree reference '" << tree_ref << "'." << std::endl;
        return;
    }

    // Read the tree object
    std::optional<std::unique_ptr<GitObject>> tree_obj_opt = object_read(repo, const_cast<char*>(tree_sha.c_str()));
    if (!tree_obj_opt.has_value()) {
        std::cerr << "Error: Could not read tree object '" << tree_sha << "'." << std::endl;
        return;
    }

    std::unique_ptr<GitObject> tree_obj = std::move(tree_obj_opt.value());
    if (tree_obj->get_fmt() != "tree") {
        std::cerr << "Error: Object '" << tree_sha << "' is not a tree." << std::endl;
        return;
    }

    // Cast to GitTree
    GitTree* tree = dynamic_cast<GitTree*>(tree_obj.get());
    if (!tree) {
        std::cerr << "Error: Could not cast object to tree." << std::endl;
        return;
    }

    // Call ls_tree helper with empty prefix
    ls_tree(repo, *tree, "", recursive);
}

void ls_tree(Repository* repo, const GitTree& tree, const std::string& prefix, bool recursive) {
    // get all leaves from the tree
    const auto& leaves = tree.get_leaves();

    // for each leaf in the tree
    for (const auto& leaf : leaves) {
        // determine the type based on the mode prefix
        std::string type;
        if (leaf.mode.size() < 2) {
            std::cerr << "Error: Mode too short: '" << leaf.mode << "'" << std::endl;
            continue;
        }
        if (leaf.mode.substr(0, 2) == "04") {
            type = "tree";
        } else if (leaf.mode.substr(0, 2) == "10") {
            type = "blob";
        } else if (leaf.mode.substr(0, 2) == "12") {
            type = "blob";
        } else if (leaf.mode.substr(0, 2) == "16") {
            type = "commit";
        } else {
            // raise error for unknown mode
            std::cerr << "Error: Unknown mode '" << leaf.mode << "' for path '"
                     << leaf.path << "'." << std::endl;
            continue;
        }

        // Construct the full path with prefix
        std::string full_path = prefix + leaf.path;

        // If recursive mode is enabled and this is a tree
        if (recursive && type == "tree") {
            // read the subtree object
            std::optional<std::unique_ptr<GitObject>> subtree_obj_opt = object_read(repo, const_cast<char*>(leaf.sha.c_str()));
            // if the subtree object exists
            if (subtree_obj_opt.has_value()) {
                std::unique_ptr<GitObject> subtree_obj = std::move(subtree_obj_opt.value());
                // if the subtree object is a tree
                if (subtree_obj->get_fmt() == "tree") {
                    // cast to GitTree
                    GitTree* subtree = dynamic_cast<GitTree*>(subtree_obj.get());
                    if (subtree) {
                        // recursively list the subtree with the new prefix
                        ls_tree(repo, *subtree, full_path + "/", recursive);
                    }
                }
            }
        // if it's a leaf
        } else {
            // print the entry: mode type sha\tpath
            std::cout << leaf.mode << " " << type << " " << leaf.sha << "\t" << full_path << std::endl;
        }
    }
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