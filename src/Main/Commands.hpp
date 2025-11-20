#pragma once
#include "CLI.hpp"
#include <iostream>

// Temp repo
class Repository;

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
void cmd_status(const ParsedArgs& args, Repository* repo);
void cmd_tag(const ParsedArgs& args, Repository* repo);