#pragma once
#include "CLI.hpp"
#include <iostream>

// Temp repo
class Repository;

// def main(argv=sys.argv[1:]):
//     args = argparser.parse_args(argv)
//     match args.command:
//         case "add"          : cmd_add(args)
//         case "cat-file"     : cmd_cat_file(args)
//         case "check-ignore" : cmd_check_ignore(args)
//         case "checkout"     : cmd_checkout(args)
//         case "commit"       : cmd_commit(args)
//         case "hash-object"  : cmd_hash_object(args)
//         case "init"         : cmd_init(args)
//         case "log"          : cmd_log(args)
//         case "ls-files"     : cmd_ls_files(args)
//         case "ls-tree"      : cmd_ls_tree(args)
//         case "rev-parse"    : cmd_rev_parse(args)
//         case "rm"           : cmd_rm(args)
//         case "show-ref"     : cmd_show_ref(args)
//         case "status"       : cmd_status(args)
//         case "tag"          : cmd_tag(args)
//         case _              : print("Bad command.")

void cmd_add(const ParsedArgs& args, Repository* repo);
void cmd_cat_file(const ParsedArgs& args, Repository* repo);
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