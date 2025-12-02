#pragma once
#include "CLI.hpp"
#include <iostream>
#include <set>
#include <string>
#include <filesystem>

// Forward declarations
class Repository;
class GitTree;

void cmd_add(const ParsedArgs& args, Repository* repo);
void cmd_cat_file(const ParsedArgs& args, Repository* repo);
void cat_file(Repository* repo, std::string object, std::string fmt);
void cmd_check_ignore(const ParsedArgs& args, Repository* repo);

/*
 * Problem: cmd_checkout
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
 * Example:
 *   Input:  args = { commit: "HEAD", path: "./output" }, repo = <valid repo>
 *   Output: Contents of HEAD commit extracted to ./output/
 *
 * Constraints:
 *   - Target path must exist and be an empty directory, OR not exist (will be created)
 *   - If commit points to a commit object, extract its tree
 *   - If commit points directly to a tree, use that tree
 */
void cmd_checkout(const ParsedArgs& args, Repository* repo);

void cmd_commit(const ParsedArgs& args, Repository* repo);
void cmd_hash_object(const ParsedArgs& args, Repository* repo);
void cmd_init(const ParsedArgs& args, Repository* repo);
void cmd_log(const ParsedArgs& args, Repository* repo);
void cmd_ls_files(const ParsedArgs& args, Repository* repo);

/*
 * Problem: cmd_ls_tree
 * ---------------------------------------------------------------------------
 * Description:
 *   Implement the bridge function for the "ls-tree" command in Silt. This
 *   command displays the contents of a tree object, showing the mode, type,
 *   SHA, and name of each entry. With the recursive flag, it traverses into
 *   subtrees and shows full paths.
 *
 * Input:
 *   - args: ParsedArgs containing:
 *       - "tree": A tree-ish reference (commit SHA, tree SHA, branch, tag, HEAD)
 *       - "recursive": Boolean flag (if true, recurse into subtrees)
 *   - repo: Pointer to the current Repository object
 *
 * Output:
 *   - Prints each tree entry in format: "<mode> <type> <sha>\t<path>"
 *   - When recursive, shows full paths like "dir/subdir/file.txt"
 *   - Subtrees are only printed as entries when NOT in recursive mode
 *
 * Example:
 *   Input:  args = { tree: "HEAD", recursive: false }
 *   Output:
 *     100644 blob abc123...  README.md
 *     040000 tree def456...  src
 *
 *   Input:  args = { tree: "HEAD", recursive: true }
 *   Output:
 *     100644 blob abc123...  README.md
 *     100644 blob ghi789...  src/main.cpp
 *
 * Constraints:
 *   - Must resolve tree-ish references to actual tree objects
 *   - Recursive mode skips printing subtree entries themselves
 */
void cmd_ls_tree(const ParsedArgs& args, Repository* repo);

void cmd_rev_parse(const ParsedArgs& args, Repository* repo);
void cmd_rm(const ParsedArgs& args, Repository* repo);
void cmd_show_ref(const ParsedArgs& args, Repository* repo);
void cmd_status(const ParsedArgs& args, Repository* repo);
void cmd_tag(const ParsedArgs& args, Repository* repo);
std::string log_graphviz(Repository* repo, std::string sha, std::set<std::string> seen);

/*
 * Problem: ls_tree
 * ---------------------------------------------------------------------------
 * Description:
 *   Implement the core logic for listing tree contents. This function takes
 *   a parsed GitTree object and prints its entries. When recursive mode is
 *   enabled, it descends into subtrees and prepends the parent path to create
 *   full paths for nested entries.
 *
 * Input:
 *   - repo: Pointer to the Repository for reading subtree objects
 *   - tree: Reference to a GitTree object to list
 *   - prefix: Current path prefix for nested entries (empty string at root)
 *   - recursive: If true, descend into subtrees; if false, show subtrees as entries
 *
 * Output:
 *   - Prints each entry to stdout in format: "<mode> <type> <sha>\t<path>"
 *   - Path includes prefix for nested entries (e.g., "src/lib/utils.cpp")
 *
 * Example:
 *   Input:  tree with entries [ {100644, "a.txt", sha1}, {040000, "dir", sha2} ]
 *           prefix = "", recursive = false
 *   Output:
 *     100644 blob sha1  a.txt
 *     040000 tree sha2  dir
 *
 *   Input:  Same tree, prefix = "parent/", recursive = false
 *   Output:
 *     100644 blob sha1  parent/a.txt
 *     040000 tree sha2  parent/dir
 *
 * Constraints:
 *   - Type is determined from mode: "04" prefix = tree, "10" = blob, etc.
 *   - Mode should be zero-padded to 6 characters in output
 */
void ls_tree(Repository *repo, const GitTree &tree, const std::string &prefix, bool recursive);


/*
 * Problem: tree_checkout
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
void tree_checkout(Repository* repo, const GitTree& tree, const std::filesystem::path& target_path);