#include "Commands.hpp"
#include "CLI.hpp"
#include <iostream>
#include <vector>
#include <string> // Include the header for GitObject and related functions
#include "Objects.hpp"
#include "Repository.hpp" // Include the header for Repository
#include "Index.hpp"
#include <filesystem>
#include <set>
#include <map>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <ctime>
#include <cstdio>
#include <sys/stat.h>
#include <zlib.h>
#include <openssl/sha.h>

// Helper to parse boolean flags from ParsedArgs:
// - If the flag is not present, return false.
// - If the flag is present but has no value, return true.
// - If a value is provided, parse common truthy strings ("true","1","yes","on")
bool parse_bool_flag(const ParsedArgs& args, const std::string& key) {
    if (!args.exists(key)) {
        return false;
    }
    std::string val = args.get(key);
    if (val.empty()) {
        return true;
    }
    std::string lower = val;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return (lower == "true" || lower == "1" || lower == "yes" || lower == "on");
}

std::string write_raw_object(const std::string& fmt, const std::string& data, Repository* repo) {
    std::string full_object = fmt + " " + std::to_string(data.size()) + '\0' + data;

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(full_object.data()), full_object.size(), hash);

    char sha_hex[41];
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        std::snprintf(sha_hex + (i * 2), 3, "%02x", hash[i]);
    }
    sha_hex[40] = '\0';
    std::string sha(sha_hex);

    std::string dir = sha.substr(0, 2);
    std::string file = sha.substr(2);
    std::filesystem::path object_path = repo_file(*repo, "objects", dir.c_str(), file.c_str(), nullptr);

    if (!std::filesystem::exists(object_path)) {
        std::filesystem::create_directories(object_path.parent_path());

        uLongf compressed_size = compressBound(full_object.size());
        std::vector<char> compressed_data(compressed_size);
        if (compress(reinterpret_cast<Bytef*>(compressed_data.data()), &compressed_size,
                     reinterpret_cast<const Bytef*>(full_object.data()), full_object.size()) != Z_OK) {
            throw std::runtime_error("Failed to compress object data.");
        }

        std::ofstream out(object_path, std::ios::binary);
        out.write(compressed_data.data(), compressed_size);
        out.close();
    }

    return sha;
}

struct TreeNode {
    std::map<std::string, TreeNode> children;
    std::map<std::string, std::pair<std::string, std::string>> files; // filename -> (mode, blob sha)
};

std::vector<std::string> split_git_path(const std::string& path) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : path) {
        if (c == '/' || c == '\\') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        parts.push_back(current);
    }
    return parts;
}

std::string write_tree_recursive(const TreeNode& node, Repository* repo) {
    std::vector<GitTreeLeaf> leaves;

    for (const auto& [name, file] : node.files) {
        leaves.emplace_back(file.first, name, file.second);
    }

    for (const auto& [dirname, child] : node.children) {
        std::string child_sha = write_tree_recursive(child, repo);
        leaves.emplace_back("40000", dirname, child_sha);
    }

    auto tree = std::make_unique<GitTree>();
    tree->set_leaves(leaves);
    return object_write(std::move(tree), repo);
}

std::string build_tree_from_index(const std::vector<IndexEntry>& entries, Repository* repo) {
    TreeNode root;

    auto mode_for_tree = [](int mode) -> std::string {
        int type = mode & 0170000;
        if (type == 0160000) {
            return "160000";
        }
        if (type == 0120000) {
            return "120000";
        }
        if (mode & 0111) {
            return "100755";
        }
        return "100644";
    };

    for (const auto& entry : entries) {
        std::vector<std::string> parts = split_git_path(entry.path);
        if (parts.empty()) {
            continue;
        }

        TreeNode* node = &root;
        for (size_t i = 0; i + 1 < parts.size(); i++) {
            node = &node->children[parts[i]];
        }
        node->files[parts.back()] = {mode_for_tree(entry.mode), entry.sha};
    }

    return write_tree_recursive(root, repo);
}

std::string head_target_ref(const Repository& repo) {
    std::ifstream head_file(repo.gitdir / "HEAD");
    std::string head_data;
    std::getline(head_file, head_data);

    if (!head_data.empty() && head_data.back() == '\r') {
        head_data.pop_back();
    }

    if (head_data.rfind("ref: ", 0) == 0) {
        return head_data.substr(5);
    }
    return "refs/heads/master";
}

std::string branch_name_from_ref(const std::string& ref_name) {
    size_t pos = ref_name.find_last_of('/');
    if (pos == std::string::npos || pos + 1 >= ref_name.size()) {
        return ref_name;
    }
    return ref_name.substr(pos + 1);
}

// Implementation of cmd_add to stage files to the index
void cmd_add(const ParsedArgs& args, Repository* repo) {
    // Get paths from positional arguments (for multiple files passed without flags)
    std::vector<std::string> paths = args.positional_args;
    
    // If no paths were provided, default to "."
    if (paths.empty()) {
        paths.push_back(".");
    }
    
    // Load or create index
    Index index(*repo);
    
    // Process each path
    for (const auto& path_str : paths) {
        std::filesystem::path path(path_str);
        
        // If path is relative, make it relative to repo worktree
        if (path.is_relative()) {
            path = repo->worktree / path;
        }
        
        // Handle directories and files recursively
        if (std::filesystem::is_directory(path)) {
            // Add all files in directory recursively
            for (const auto& file_entry : std::filesystem::recursive_directory_iterator(path)) {
                if (file_entry.is_regular_file()) {
                    // Get path relative to worktree
                    auto rel_path = std::filesystem::relative(file_entry.path(), repo->worktree);
                    
                    // Skip files in .git directory
                    if (rel_path.string().find(".git") == 0) {
                        continue;
                    }
                    
                    // Read file and compute hash
                    std::ifstream file(file_entry.path(), std::ios::binary);
                    std::string content((std::istreambuf_iterator<char>(file)),
                                       std::istreambuf_iterator<char>());
                    file.close();
                    
                    // Hash the blob
                    std::string sha = object_hash(content, "blob", repo);
                    
                    // Create index entry
                    IndexEntry index_entry;
                    index_entry.path = rel_path.generic_string();
                    
                    // Get file stats using the actual file path
                    struct stat st;
                    if (stat(file_entry.path().string().c_str(), &st) == 0) {
                        index_entry.ctime_sec = static_cast<int>(st.st_ctime);
                        index_entry.mtime_sec = static_cast<int>(st.st_mtime);
                        index_entry.mode = static_cast<int>(st.st_mode);
                        index_entry.uid = static_cast<int>(st.st_uid);
                        index_entry.gid = static_cast<int>(st.st_gid);
                        index_entry.file_size = static_cast<int>(st.st_size);
                    }
                    
                    index_entry.sha = sha;
                    index_entry.flags = static_cast<int>(index_entry.path.size() & 0xFFF); // Bit 0-11: name length
                    
                    // Add to index
                    index.add_entry(index_entry);
                }
            }
        } else if (std::filesystem::is_regular_file(path)) {
            // Single file
            auto rel_path = std::filesystem::relative(path, repo->worktree);
            
            // Read file and compute hash
            std::ifstream file(path, std::ios::binary);
            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            file.close();
            
            // Hash the blob
            std::string sha = object_hash(content, "blob", repo);
            
            // Create index entry
            IndexEntry entry;
            entry.path = rel_path.generic_string();
            
            // Get file stats
            struct stat st;
            if (stat(path.string().c_str(), &st) == 0) {
                entry.ctime_sec = static_cast<int>(st.st_ctime);
                entry.mtime_sec = static_cast<int>(st.st_mtime);
                entry.mode = static_cast<int>(st.st_mode);
                entry.uid = static_cast<int>(st.st_uid);
                entry.gid = static_cast<int>(st.st_gid);
                entry.file_size = static_cast<int>(st.st_size);
            }
            
            entry.sha = sha;
            entry.flags = static_cast<int>(entry.path.size() & 0xFFF);
            
            // Add to index
            index.add_entry(entry);
        }
    }
    
    // Write index back to disk
    if (index.write(*repo)) {
        std::cout << "Index updated." << std::endl;
    } else {
        std::cerr << "Error: Failed to write index." << std::endl;
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

void cmd_checkout(const ParsedArgs& args, Repository* repo) {
    // get the commit reference and path
    std::string commit_ref = args.get("commit");
    std::string path_arg = args.get("path");

    // if the commit or path is empty, print error and return
    if (commit_ref.empty()) {
        std::cerr << "Error: commit argument is required." << std::endl;
        return;
    }

    if (path_arg.empty()) {
        std::cerr << "Error: path argument is required." << std::endl;
        return;
    }

    // find the repository if not provided
    std::optional<Repository> found_repo;
    if (!repo) {
        found_repo = repo_find(std::filesystem::current_path(), true);
        if (found_repo.has_value()) {
            repo = &found_repo.value();
        } else {
            std::cerr << "Error: Not a Git repository." << std::endl;
            return;
        }
    }

    // resolve commit reference to SHA
    std::string resolved = object_find(repo, commit_ref, "", true);
    if (resolved.empty()) {
        std::cerr << "Error: Could not resolve reference '" << commit_ref << "'." << std::endl;
        return;
    }

    // read the object
    auto obj_opt = object_read(repo, const_cast<char*>(resolved.c_str()));
    // if the object is not found, print error and return
    if (!obj_opt.has_value()) {
        std::cerr << "Error: Could not read object '" << resolved << "'." << std::endl;
        return;
    }

    // determine if it's a commit or tree
    std::unique_ptr<GitObject> obj = std::move(obj_opt.value());
    std::unique_ptr<GitObject> tree_obj;

    // if the object is a commit
    if (obj->get_fmt() == "commit") {
        // cast to GitCommit
        GitCommit* commit = dynamic_cast<GitCommit*>(obj.get());
        if (!commit) {
            // if the cast fails, print error and return
            std::cerr << "Error: Failed to interpret commit object." << std::endl;
            return;
        }

        // parse the commit
        KVLM kvlm = kvlm_parse(commit->serialize());
        // find the tree
        auto tree_it = kvlm.find("tree");
        // if the tree is not found, print error and return
        if (tree_it == kvlm.end() || !std::holds_alternative<std::string>(tree_it->second)) {
            std::cerr << "Error: Commit does not contain a tree." << std::endl;
            return;
        }

        // read the tree object
        std::string tree_sha = std::get<std::string>(tree_it->second);
        auto tree_opt = object_read(repo, const_cast<char*>(tree_sha.c_str()));
        if (!tree_opt.has_value()) {
            // if the tree object is not found, print error and return
            std::cerr << "Error: Could not read tree '" << tree_sha << "'." << std::endl;
            return;
        }
        tree_obj = std::move(tree_opt.value());
    // if the object is a tree
    } else if (obj->get_fmt() == "tree") {
        tree_obj = std::move(obj);
    // else, print error and return
    } else {
        std::cerr << "Error: Object '" << resolved << "' is not a commit or tree." << std::endl;
        return;
    }

    // cast tree_obj to GitTree
    GitTree* tree = dynamic_cast<GitTree*>(tree_obj.get());
    if (!tree) {
        std::cerr << "Error: Failed to interpret target tree." << std::endl;
        return;
    }

    // prepare target path
    std::filesystem::path target_path = path_arg;
    try {
        // if the target path exists, check if it's a directory and empty
        if (std::filesystem::exists(target_path)) {
            // if it's not a directory
            if (!std::filesystem::is_directory(target_path)) {
                std::cerr << "Error: Target path must be a directory." << std::endl;
                return;
            }
            // if it's not empty
            if (!std::filesystem::is_empty(target_path)) {
                std::cerr << "Error: Target directory must be empty." << std::endl;
                return;
            }
        } else {
            std::filesystem::create_directories(target_path);
        }
    // catch any filesystem errors
    } catch (const std::exception& e) {
        std::cerr << "Error preparing target path: " << e.what() << std::endl;
        return;
    }

    // perform checkout
    tree_checkout(repo, *tree, target_path);
}

void tree_checkout(Repository* repo, const GitTree& tree, const std::filesystem::path& target_path) {
    // get the leaves of the tree
    const auto& leaves = tree.get_leaves();

    // for each leaf in the tree
    for (const auto& leaf : leaves) {
        // read the object
        auto obj_opt = object_read(repo, const_cast<char*>(leaf.sha.c_str()));
        // if the object is not found, print warning and continue
        if (!obj_opt.has_value()) {
            std::cerr << "Warning: Unable to read object '" << leaf.sha << "'." << std::endl;
            continue;
        }

        // process based on object type
        std::unique_ptr<GitObject> obj = std::move(obj_opt.value());
        std::filesystem::path destination = target_path / leaf.path;

        // if the object is a tree, recurse
        if (obj->get_fmt() == "tree") {
            try {
                // create the directory
                std::filesystem::create_directories(destination);
            } catch (const std::exception& e) {
                // if the directory could not be created, print warning and continue
                std::cerr << "Warning: Could not create directory '" << destination.string() << "': " << e.what() << std::endl;
                continue;
            }

            // cast the object to a tree
            GitTree* subtree = dynamic_cast<GitTree*>(obj.get());
            // if the cast was successful, recurse
            if (subtree) {
                tree_checkout(repo, *subtree, destination);
            }
        // if the object is a blob, write to file
        } else if (obj->get_fmt() == "blob") {
            // ensure parent directories exist
            if (destination.has_parent_path()) {
                std::error_code ec;
                std::filesystem::create_directories(destination.parent_path(), ec);
            }

            // write blob data to file
            std::ofstream file(destination, std::ios::binary);

            // if file could not be opened, print warning and continue
            if (!file.is_open()) {
                std::cerr << "Warning: Could not write file '" << destination.string() << "'." << std::endl;
                continue;
            }

            // serialize the object and write to file
            std::string data = obj->serialize();
            file.write(data.data(), data.size());
        } else {
            // unsupported object type
            std::cerr << "Warning: Unsupported object type '" << obj->get_fmt() << "' for path '" << leaf.path << "'." << std::endl;
        }
    }
}

void cmd_commit(const ParsedArgs& args, Repository* repo) {
    // Get commit message
    std::string message = args.get("message");
    
    if (message.empty()) {
        std::cerr << "Error: Commit message required (-m flag)" << std::endl;
        return;
    }
    
    // Load index
    Index index(*repo);
    const auto& entries = index.get_entries();
    
    if (entries.empty()) {
        std::cerr << "Error: nothing to commit" << std::endl;
        return;
    }
    
    // Build nested trees from index paths and write the root tree object.
    std::string tree_sha = build_tree_from_index(entries, repo);
    
    // Get parent commit (HEAD)
    auto head_ref = ref_resolve(*repo, "HEAD");
    std::string parent_sha;
    if (head_ref.has_value()) {
        parent_sha = head_ref.value();
    }
    
    // Create commit object with Git-compatible header order:
    // tree, parent(s), author, committer, blank line, message
    std::string author_line = "Silt User <silt@example.com> " + std::to_string(std::time(nullptr)) + " +0000";
    std::string commit_data = "tree " + tree_sha + "\n";
    if (!parent_sha.empty()) {
        commit_data += "parent " + parent_sha + "\n";
    }
    commit_data += "author " + author_line + "\n";
    commit_data += "committer " + author_line + "\n\n";
    commit_data += message + "\n";
    
    // Write commit object
    std::string commit_sha = write_raw_object("commit", commit_data, repo);
    
    // Update the reference that HEAD points to (e.g. refs/heads/main).
    std::string target_ref = head_target_ref(*repo);
    ref_create(repo, target_ref, commit_sha);
    
    std::cout << "[" << branch_name_from_ref(target_ref) << " " << commit_sha.substr(0, 7) << "] " << message << std::endl;
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
    // Load index
    Index index(*repo);
    
    // Get all entries
    const auto& entries = index.get_entries();
    
    // Print each entry
    for (const auto& entry : entries) {
        // Print: [mode] [object] [stage] [file]
        // Format similar to: 100644 blob_sha 0	filename
        printf("%06o %s %d\t%s\n", entry.mode, entry.sha.substr(0, 7).c_str(), 0, entry.path.c_str());
    }
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
    std::string fmt = "";
    // if the user gave a type
    if (args.exists("type")) {
        // use the type, otherwise use the default
        fmt = args.get("type");
    }

    // call object_find, this prints the sha of the object
    std::cout << object_find(repo, args.get("name"), fmt, true) << std::endl;
}

void cmd_show_ref(const ParsedArgs& args, Repository* repo) {
    // get repo
    std::map<std::string, std::string> refs = ref_list(*repo, std::filesystem::path());
    // call show ref
    show_ref(repo, refs, true, "refs");
}

void show_ref(Repository* repo, const std::map<std::string, std::string>& refs, bool with_hash, const std::string& prefix) {
    // for every key val in refs
    for (const auto& [path, sha] : refs) {
        if (with_hash && !sha.empty()) {
            std::cout << sha << " ";
        }
        std::cout << path << std::endl;
    }
}

void cmd_status(const ParsedArgs& args, Repository* repo) {
    // Load index
    Index index(*repo);
    
    // Get current HEAD commit
    auto head_ref = ref_resolve(*repo, "HEAD");
    std::string head_sha;
    if (head_ref.has_value()) {
        head_sha = head_ref.value();
    }
    
    std::cout << "On branch master" << std::endl;
    
    // Section 1: Changes to be committed (index vs HEAD)
    bool has_staged = false;
    if (!head_sha.empty()) {
        auto commit_obj = object_read(repo, const_cast<char*>(head_sha.c_str()));
        if (commit_obj.has_value()) {
            GitCommit* commit = dynamic_cast<GitCommit*>(commit_obj->get());
            if (commit) {
                // Would compare index with HEAD tree
                // For now, just list index entries as staged
                const auto& entries = index.get_entries();
                if (!entries.empty()) {
                    has_staged = true;
                    std::cout << "\nChanges to be committed:" << std::endl;
                    std::cout << "  (use \"git reset HEAD <file>...\" to unstage)" << std::endl;
                    for (const auto& entry : entries) {
                        std::cout << "\tmodified:   " << entry.path << std::endl;
                    }
                }
            }
        }
    } else {
        // Initial commit - all staged files are new
        const auto& entries = index.get_entries();
        if (!entries.empty()) {
            has_staged = true;
            std::cout << "\nChanges to be committed:" << std::endl;
            std::cout << "  (use \"git reset HEAD <file>...\" to unstage)" << std::endl;
            for (const auto& entry : entries) {
                std::cout << "\tnew file:   " << entry.path << std::endl;
            }
        }
    }
    
    // Section 2: Changes not staged (worktree vs index)
    bool has_unstaged = false;
    const auto& entries = index.get_entries();
    for (const auto& entry : entries) {
        std::filesystem::path file_path = repo->worktree / entry.path;
        
        // Check if file still exists
        if (!std::filesystem::exists(file_path)) {
            if (!has_unstaged) {
                has_unstaged = true;
                std::cout << "\nChanges not staged for commit:" << std::endl;
                std::cout << "  (use \"git add <file>...\" to update what will be committed)" << std::endl;
            }
            std::cout << "\tdeleted:    " << entry.path << std::endl;
        } else {
            // Check if file content changed
            std::ifstream file(file_path, std::ios::binary);
            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            file.close();
            
            std::string sha = object_hash(content, "blob", nullptr);
            if (sha != entry.sha) {
                if (!has_unstaged) {
                    has_unstaged = true;
                    std::cout << "\nChanges not staged for commit:" << std::endl;
                    std::cout << "  (use \"git add <file>...\" to update what will be committed)" << std::endl;
                }
                std::cout << "\tmodified:   " << entry.path << std::endl;
            }
        }
    }
    
    // Section 3: Untracked files
    bool has_untracked = false;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(repo->worktree)) {
        if (entry.is_regular_file()) {
            auto rel_path = std::filesystem::relative(entry.path(), repo->worktree);
            
            // Skip .git directory and files already in index
            if (rel_path.string().find(".git") == 0) {
                continue;
            }
            
            auto index_entry = index.get_entry(rel_path.string());
            if (!index_entry.has_value()) {
                if (!has_untracked) {
                    has_untracked = true;
                    std::cout << "\nUntracked files:" << std::endl;
                    std::cout << "  (use \"git add <file>...\" to include in what will be committed)" << std::endl;
                }
                std::cout << "\t" << rel_path.string() << std::endl;
            }
        }
    }
    
    // Summary
    if (!has_staged && !has_unstaged && !has_untracked) {
        std::cout << "\nnothing to commit, working tree clean" << std::endl;
    }
}

void cmd_tag(const ParsedArgs& args, Repository* repo) {
    std::string name = args.get("name");
    std::string object = args.get("object");

    // Handle positional arguments if name is not provided via flag
    if (name.empty() && !args.positional_args.empty()) {
        name = args.positional_args[0];
        if (args.positional_args.size() > 1) {
            object = args.positional_args[1];
        }
    }

    // if name is not empty
    if (!name.empty()) {
        bool append_flag = parse_bool_flag(args, "annotate");
        tag_create(repo, name, object, append_flag);
    } else {
        std::map<std::string, std::string> refs = ref_list(*repo, repo->gitdir / "refs" / "tags");
        // for every key val in refs
        for (const auto& [path, sha] : refs) {
            // print only the tag name (remove refs/tags/)
            // path is relative to .git, so it is refs/tags/name
            if (path.rfind("refs/tags/", 0) == 0) {
                std::cout << path.substr(10) << std::endl;
            } else {
                std::cout << path << std::endl;
            }
        }
    }

}

// tag_create(repo, name, ref, create_tag_object=False)
void tag_create(Repository* repo, const std::string& name, const std::string& ref, bool create_tag_object) {
    // object find sha using repo and ref
    std::string sha = object_find(repo, ref, "commit", true);

    if (create_tag_object) {
        // create a GitTag object
        auto tag = std::make_unique<GitTag>();

        // prepare the KVLM data for the tag
        KVLM kvlm;
        kvlm["object"] = sha;
        kvlm["type"] = "commit";
        kvlm["tag"] = name;
        kvlm["tagger"] = "silt <silt@example.com>";
        kvlm[""] = "Some message, change later maybe?";

        // Set the kvlm for the tag
        std::string serialized_tag = kvlm_serialize(kvlm);
        tag->deserialize(serialized_tag);

        // write the tag object via object_write
        std::string tag_sha = object_write(std::move(tag), repo);

        // create ref via ref_create, pass repo, "refs/tags/" + name, tag_sha
        ref_create(repo, "refs/tags/" + name, tag_sha);
    } else {
        // create ref via ref_create, pass repo, "refs/tags/" + name, sha
        ref_create(repo, "refs/tags/" + name, sha);
    }
}

// ref_create(repo, ref_name, sha)
void ref_create(Repository* repo, const std::string& ref_name, const std::string& sha) {
    // Open repo_file(repo, ref_name) and write sha
    std::filesystem::path ref_path = repo_file(*repo, ref_name.c_str(), nullptr);

    // Write refs with LF-only to stay compatible with Git parsing on Windows.
    std::ofstream file(ref_path, std::ios::binary);
    file << sha << "\n";
    file.close();
}
