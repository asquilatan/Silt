#pragma once
#include "CLI.hpp"
#include <iostream>
#include <set>
#include <string>
#include <filesystem>
#include <map>
#include <variant>

// Forward declarations
class Repository;
class GitTree;

void cmd_add(const ParsedArgs& args, Repository* repo);
void cmd_cat_file(const ParsedArgs& args, Repository* repo);
void cat_file(Repository* repo, std::string object, std::string fmt);
void cmd_check_ignore(const ParsedArgs& args, Repository* repo);
void cmd_checkout(const ParsedArgs& args, Repository* repo);

void cmd_commit(const ParsedArgs& args, Repository* repo);
void cmd_hash_object(const ParsedArgs& args, Repository* repo);
void cmd_init(const ParsedArgs& args, Repository* repo);
void cmd_log(const ParsedArgs& args, Repository* repo);
void cmd_ls_files(const ParsedArgs& args, Repository* repo);
void cmd_ls_tree(const ParsedArgs& args, Repository* repo);
void cmd_rev_parse(const ParsedArgs& args, Repository* repo);
void cmd_rm(const ParsedArgs& args, Repository* repo);

void cmd_show_ref(const ParsedArgs& args, Repository* repo);
void show_ref(Repository* repo, const std::map<std::string, std::string>& refs, bool with_hash = true, const std::string& prefix="");

void cmd_status(const ParsedArgs& args, Repository* repo);
void cmd_tag(const ParsedArgs& args, Repository* repo);
void tag_create(Repository* repo, const std::string& name, const std::string& ref, bool create_tag_object);
void ref_create(Repository* repo, const std::string& ref_name, const std::string& sha);
std::string log_graphviz(Repository* repo, std::string sha, std::set<std::string> seen);
void ls_tree(Repository *repo, const GitTree &tree, const std::string &prefix, bool recursive);
void tree_checkout(Repository* repo, const GitTree& tree, const std::filesystem::path& target_path);
