#ifndef REPOSITORY_HPP
#define REPOSITORY_HPP

#include <string>
#include <filesystem>
#include <cstdarg>

// Forward declaration
class ConfigParser;

class Repository {
public:
    std::filesystem::path worktree; // The root of the user's files
    std::filesystem::path gitdir;   // The .git directory
    std::filesystem::path conf;
    bool force;

    // If gitdir is not a repo, raise an exception
    Repository(const std::filesystem::path& path, bool force = false);
};

// Forward declarations for repo functions
std::filesystem::path repo_path(const Repository& repo, const char* first, ...);
std::filesystem::path repo_file(const Repository& repo, const char* first, ...);
std::filesystem::path repo_dir(const Repository& repo, bool create, const char* first, ...);
Repository repo_create(const std::filesystem::path& path);
std::string repo_default_config();

#endif // REPOSITORY_HPP