#include <string>
#include <filesystem>
#include <stdexcept>
#include <iostream>
#include <map>
#include <cstdarg>
#include <fstream>
#include "Repository.hpp"
#include "Utils.hpp"




// Repository constructor implementation
Repository::Repository(const std::filesystem::path& path, bool force) : worktree(path), gitdir(path / ".git"), force(force), conf() {
    if (!(force) && !(std::filesystem::is_directory(gitdir))) {
        throw std::runtime_error("Not a Git repository " + path.string());
    }

    // Read config file in .git/config
    ConfigParser conf_parser;
    // The repo_file function needs a repository object and a nullptr to terminate the variadic arguments.
    std::filesystem::path cf = repo_path(*this, "config", nullptr);

    if (std::filesystem::exists(cf)) {
        conf_parser.read(cf.string());
        this->conf = cf;
    } else if (!force) {
        throw std::runtime_error("Configuration file missing");
    }

    if (!force) {
        int vers = std::stoi(conf_parser.get("core", "repositoryformatversion", "0"));
        if (vers != 0) {
            throw std::runtime_error("Unsupported repository format version: " + std::to_string(vers));
        }
    }
}



// Joins path components to the repository's git directory.
std::filesystem::path repo_path(const Repository& repo, const char* first, ...) {
    // Takes the path of the git directory
    std::filesystem::path path = repo.gitdir;

    // Expands the args of first
    va_list args;
    va_start(args, first);

    // Process the first argument
    if (first) {
        // Add first to the path
        path /= first;
        // Process subsequent arguments
        const char* current = va_arg(args, const char*);
        while (current) {
            path /= current;
            current = va_arg(args, const char*);
        }
    }

    va_end(args);
    return path;
}

// Same as repo_path, but creates the parent directory if it doesn't exist.
std::filesystem::path repo_file(const Repository& repo, const char* first, ...) {
    // Path of git dir
    std::filesystem::path path = repo.gitdir;

    va_list args;
    va_start(args, first);

    // Process first
    if (first) {
        path /= first;
        // Process subsequent args
        const char* current = va_arg(args, const char*);
        while (current) {
            path /= current;
            current = va_arg(args, const char*);
        }
    }

    va_end(args);

    // Create parent directory if it doesn't exist
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    return path;
}

// Same as repo_path, but creates the directory itself if it doesn't exist.
std::filesystem::path repo_dir(const Repository& repo, bool create, const char* first, ...) {
    std::filesystem::path path = repo.gitdir;

    va_list args;
    va_start(args, first);

    if (first) {
        path /= first;
        const char* current = va_arg(args, const char*);
        while (current) {
            path /= current;
            current = va_arg(args, const char*);
        }
    }

    va_end(args);

    // If path exists
    if (std::filesystem::exists(path)) {
        // If path is a directory
        if (std::filesystem::is_directory(path)) {
            return path;
        // If path is not a directory
        } else {
            throw std::runtime_error("Not a directory " + path.string());
        }
    }

    // if mkdir
    if (create) {
        // make a dir on path
        std::filesystem::create_directories(path);
        return path;
    }

    // else, return none
    return std::filesystem::path();
}


// Creates a new repository at the given path. this should only called once, by cmd_init
Repository repo_create(const std::filesystem::path& path) {
    Repository repo(path, true);

    // If the work tree exists
    if (std::filesystem::exists(repo.worktree)) {
        // and if the work tree is NOT a directory
        if (!std::filesystem::is_directory(repo.worktree)) {
            throw std::runtime_error("Not a directory " + repo.worktree.string());
        }
        // if there is already a git directory
        if (std::filesystem::exists(repo.gitdir) && std::filesystem::is_directory(repo.gitdir)) {
            throw std::runtime_error(repo.gitdir.string() + " is not empty.");
        }
    // Otherwise, create dir
    } else {
        std::filesystem::create_directories(repo.worktree);
    }

    // Create the git directory
    repo_dir(repo, true, "branches", nullptr);
    repo_dir(repo, true, "objects", nullptr);
    repo_dir(repo, true, "refs", "heads", nullptr);
    repo_dir(repo, true, "refs", "tags", nullptr);

    // Create the config file
    std::filesystem::path description_path = repo_file(repo, "description", nullptr);
    // If description_file was created, write the description
    std::ofstream description_file(description_path);
    if (description_file.is_open()) {
        description_file << "Unnamed repository; edit this file 'description' to name the repository.\n";
        description_file.close();
        // Otherwise, throw an error
    } else {
        throw std::runtime_error("Failed to create description file: " + description_path.string());
    }

    // Create the HEAD file
    std::filesystem::path head_path = repo_file(repo, "HEAD", nullptr);
    // If the HEAD was created, write the reference to the current head
    std::ofstream head_file(head_path);
    if (head_file.is_open()) {
        head_file << "ref: refs/heads/master\n";
        head_file.close();
    } else {
        throw std::runtime_error("Failed to create HEAD file: " + head_path.string());
    }

    // Create the config file
    std::filesystem::path config_path = repo_file(repo, "config", nullptr);
    std::ofstream config_file(config_path);
    if (config_file.is_open()) {
        config_file << repo_default_config();
        config_file.close();
    } else {
        throw std::runtime_error("Failed to create config file: " + config_path.string());
    }

    return repo;
}

// Default config, use config parser
std::string repo_default_config() {
    ConfigParser config;
    config.set("core", "repositoryformatversion", "0");
    config.set("core", "filemode", "false");
    config.set("core", "bare", "false");

    return config.toString();
}

std::optional<Repository> repo_find(std::filesystem::path path, bool required) {
    // takes real path of a given path
    path = std::filesystem::canonical(path);

    // if the directory is a git repository
    std::filesystem::path git_path = path / ".git";
    if (std::filesystem::is_directory(git_path)) {
        return Repository(path, false);
    }

    // if it isn't a git repo, recurse to parent; assign parent path to parent
    std::filesystem::path parent = path.parent_path();

    // if parent is path
    if (parent == path) {
        // if required
        if (required) {
            // raise exception
            throw std::runtime_error("No Git repository found");
        }
        // otherwise, return none
        return std::nullopt;
    }

    // recurse, pass parent
    return repo_find(parent, required);
}


// resolve the reference to its canonical value
// for example:
// input: repo, "HEAD" (HEAD points to ref: refs/heads/master, which points to a94a8fe2b1cd9...)
// output: "a94a8fe2b1cd9..."
// input: repo, "refs/heads/master"
// output: "a94a8fe2b1cd9..."

std::optional<std::string> ref_resolve(const Repository& repo, const std::string& ref) {
    // get the path via repo file
    std::filesystem::path path = repo.gitdir / ref;
    
    // if the path is not a file , return nothing
    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }

    // read the file and 
    std::ifstream file(path);
    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // trim trailing newline
    if (!data.empty() && (data.back() == '\n' || data.back() == '\r')) {
        data.pop_back();
    }
    
    // if it starts with ref: return ref_resolve on repo and data starting from the 6th character
    if (data.rfind("ref: ", 0) == 0) {
        return ref_resolve(repo, data.substr(5));
        // else return data
    } else {
        return data;
    }
}

std::map<std::string, std::string> ref_list(const Repository& repo, const std::filesystem::path& path_prefix) {
    
    std::filesystem::path start_path;
    // if path doesn't exist
    if (path_prefix.empty()) {
        // let path be repo_dir repo with refs
        start_path = repo.gitdir / "refs";
        } else {
            start_path = path_prefix;
        }
        
        // create a hashmap, assign to return value
        std::map<std::string, std::string> refs;
        if (!std::filesystem::exists(start_path)) {
            return refs;
        }
        // for each file in the sorted directories in path
        for (const auto& entry : std::filesystem::directory_iterator(start_path)) {
            std::filesystem::path full_path = entry.path();
            std::string relative_path = std::filesystem::relative(full_path, repo.gitdir).string();
            std::replace(relative_path.begin(), relative_path.end(), '\\', '/');
            
            // if the joined path is a directory
            if (std::filesystem::is_directory(full_path)) {
                // recursively call ref_list on repo and joined path, assign to hashmap at file
                refs.merge(ref_list(repo, full_path));
            // else
            } else {
                // call ref_resolve on repo and joined path, assign to hashmap at file
                auto resolved_sha = ref_resolve(repo, relative_path);
                if(resolved_sha)
                refs[relative_path] = *resolved_sha;
            }
        }
    // return hashmap
    return refs;
}