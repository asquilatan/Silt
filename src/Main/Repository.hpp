#ifndef REPOSITORY_HPP
#define REPOSITORY_HPP

#include <map>
#include <string>
#include <filesystem>
#include <cstdarg>
#include <optional>

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
std::optional<Repository> repo_find(std::filesystem::path path = ".", bool required = true);
std::string repo_default_config();
std::optional<std::string> ref_resolve(const Repository& repo, const std::string& ref);
std::map<std::string, std::string> ref_list(const Repository& repo, const std::filesystem::path& path_prefix = "");


#endif // REPOSITORY_HPP