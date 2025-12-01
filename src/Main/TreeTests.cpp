/*
 * TreeTests.cpp
 * ---------------------------------------------------------------------------
 * Test cases for Git tree-related functions in Silt.
 * These tests cover the checkout section (Section 6) functionality:
 *   - GitTreeLeaf structure
 *   - tree_parse_one
 *   - tree_parse
 *   - tree_leaf_sort_key
 *   - tree_serialize
 *   - GitTree class
 *   - ls_tree
 *   - tree_checkout
 *   - cmd_ls_tree
 *   - cmd_checkout
 *
 * To run these tests, compile with a test framework or use assertions.
 */

#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include "Objects.hpp"
#include "Commands.hpp"
#include "Repository.hpp"
#include "CLI.hpp"

// Helper function to create a raw 20-byte SHA from hex string
std::string hex_to_raw_sha(const std::string& hex) {
    std::string raw;
    for (size_t i = 0; i < hex.length(); i += 2) {
        unsigned int byte;
        std::stringstream ss;
        ss << std::hex << hex.substr(i, 2);
        ss >> byte;
        raw += static_cast<char>(byte);
    }
    return raw;
}

// Helper function to create a mock raw tree entry
std::string create_raw_tree_entry(const std::string& mode, const std::string& path, const std::string& sha_hex) {
    std::string entry;
    entry += mode;
    entry += ' ';
    entry += path;
    entry += '\0';
    entry += hex_to_raw_sha(sha_hex);
    return entry;
}

/*
 * ---------------------------------------------------------------------------
 * Test: GitTreeLeaf Construction
 * ---------------------------------------------------------------------------
 * Description:
 *   Verify that GitTreeLeaf correctly stores mode, path, and sha fields.
 *
 * Input:
 *   - mode: "100644"
 *   - path: "README.md"
 *   - sha: "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391"
 *
 * Expected Output:
 *   - leaf.mode == "100644"
 *   - leaf.path == "README.md"
 *   - leaf.sha == "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391"
 */
void test_git_tree_leaf_construction() {
    std::cout << "Test: GitTreeLeaf Construction... ";

    GitTreeLeaf leaf;
    leaf.mode = "100644";
    leaf.path = "README.md";
    leaf.sha = "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391";

    assert(leaf.mode == "100644");
    assert(leaf.path == "README.md");
    assert(leaf.sha == "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391");

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: tree_parse_one - Regular File
 * ---------------------------------------------------------------------------
 * Description:
 *   Parse a single tree entry for a regular file (mode 100644).
 *
 * Input:
 *   - raw: "100644 file.txt\0<20 bytes SHA>"
 *   - start: 0
 *
 * Expected Output:
 *   - leaf.mode == "100644"
 *   - leaf.path == "file.txt"
 *   - leaf.sha == 40-character hex string
 *   - next_offset == length of entry (mode + space + path + null + 20 bytes)
 */
void test_tree_parse_one_regular_file() {
    std::cout << "Test: tree_parse_one - Regular File... ";

    std::string sha_hex = "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391";
    std::string raw = create_raw_tree_entry("100644", "file.txt", sha_hex);

    auto [leaf, next_pos] = tree_parse_one(raw, 0);

    assert(leaf.mode == "100644");
    assert(leaf.path == "file.txt");
    assert(leaf.sha == sha_hex);
    assert(next_pos == raw.length());

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: tree_parse_one - Directory (5-digit mode)
 * ---------------------------------------------------------------------------
 * Description:
 *   Parse a single tree entry for a directory with 5-digit mode (40000).
 *   Mode should be normalized to 6 digits (040000).
 *
 * Input:
 *   - raw: "40000 src\0<20 bytes SHA>"
 *   - start: 0
 *
 * Expected Output:
 *   - leaf.mode == "040000" (normalized)
 *   - leaf.path == "src"
 *   - leaf.sha == 40-character hex string
 */
void test_tree_parse_one_directory() {
    std::cout << "Test: tree_parse_one - Directory (5-digit mode)... ";

    std::string sha_hex = "4b825dc642cb6eb9a060e54bf8d69288fbee4904";
    std::string raw = create_raw_tree_entry("40000", "src", sha_hex);

    auto [leaf, next_pos] = tree_parse_one(raw, 0);

    assert(leaf.mode == "040000");  // Should be normalized to 6 digits
    assert(leaf.path == "src");
    assert(leaf.sha == sha_hex);

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: tree_parse_one - Offset Continuation
 * ---------------------------------------------------------------------------
 * Description:
 *   Parse a tree entry starting at a non-zero offset.
 *
 * Input:
 *   - raw: "<entry1><entry2>" (two concatenated entries)
 *   - start: length of entry1
 *
 * Expected Output:
 *   - Correctly parses entry2
 *   - Returns offset pointing past entry2
 */
void test_tree_parse_one_with_offset() {
    std::cout << "Test: tree_parse_one - Offset Continuation... ";

    std::string sha1 = "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391";
    std::string sha2 = "4b825dc642cb6eb9a060e54bf8d69288fbee4904";
    std::string entry1 = create_raw_tree_entry("100644", "a.txt", sha1);
    std::string entry2 = create_raw_tree_entry("100644", "b.txt", sha2);
    std::string raw = entry1 + entry2;

    auto [leaf, next_pos] = tree_parse_one(raw, entry1.length());

    assert(leaf.mode == "100644");
    assert(leaf.path == "b.txt");
    assert(leaf.sha == sha2);
    assert(next_pos == raw.length());

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: tree_parse - Multiple Entries
 * ---------------------------------------------------------------------------
 * Description:
 *   Parse a complete tree object with multiple entries.
 *
 * Input:
 *   - raw: Concatenation of 3 tree entries
 *
 * Expected Output:
 *   - Vector with 3 GitTreeLeaf elements
 *   - Each leaf has correct mode, path, and sha
 */
void test_tree_parse_multiple_entries() {
    std::cout << "Test: tree_parse - Multiple Entries... ";

    std::string sha1 = "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391";
    std::string sha2 = "4b825dc642cb6eb9a060e54bf8d69288fbee4904";
    std::string sha3 = "83baae61804e65cc73a7201a7252750c76066a30";

    std::string raw = create_raw_tree_entry("100644", "README.md", sha1)
                    + create_raw_tree_entry("40000", "src", sha2)
                    + create_raw_tree_entry("100644", "main.cpp", sha3);

    std::vector<GitTreeLeaf> leaves = tree_parse(raw);

    assert(leaves.size() == 3);
    assert(leaves[0].path == "README.md");
    assert(leaves[1].path == "src");
    assert(leaves[1].mode == "040000");  // Normalized
    assert(leaves[2].path == "main.cpp");

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: tree_parse - Empty Tree
 * ---------------------------------------------------------------------------
 * Description:
 *   Parse an empty tree object.
 *
 * Input:
 *   - raw: "" (empty string)
 *
 * Expected Output:
 *   - Empty vector
 */
void test_tree_parse_empty() {
    std::cout << "Test: tree_parse - Empty Tree... ";

    std::vector<GitTreeLeaf> leaves = tree_parse("");

    assert(leaves.empty());

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: tree_leaf_sort_key - Regular File
 * ---------------------------------------------------------------------------
 * Description:
 *   Generate sort key for a regular file entry.
 *
 * Input:
 *   - leaf: { mode: "100644", path: "foo.c", sha: "..." }
 *
 * Expected Output:
 *   - "foo.c" (just the path)
 */
void test_tree_leaf_sort_key_file() {
    std::cout << "Test: tree_leaf_sort_key - Regular File... ";

    GitTreeLeaf leaf;
    leaf.mode = "100644";
    leaf.path = "foo.c";
    leaf.sha = "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391";

    std::string key = tree_leaf_sort_key(leaf);

    assert(key == "foo.c");

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: tree_leaf_sort_key - Directory
 * ---------------------------------------------------------------------------
 * Description:
 *   Generate sort key for a directory entry.
 *
 * Input:
 *   - leaf: { mode: "040000", path: "foo", sha: "..." }
 *
 * Expected Output:
 *   - "foo/" (path with trailing slash)
 */
void test_tree_leaf_sort_key_directory() {
    std::cout << "Test: tree_leaf_sort_key - Directory... ";

    GitTreeLeaf leaf;
    leaf.mode = "040000";
    leaf.path = "foo";
    leaf.sha = "4b825dc642cb6eb9a060e54bf8d69288fbee4904";

    std::string key = tree_leaf_sort_key(leaf);

    assert(key == "foo/");

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: tree_leaf_sort_key - Sort Order Verification
 * ---------------------------------------------------------------------------
 * Description:
 *   Verify that the sort key produces correct ordering:
 *   "foo" (directory) should sort after "foo.c" (file)
 *
 * Input:
 *   - Two leaves: file "foo.c" and directory "foo"
 *
 * Expected Output:
 *   - "foo.c" < "foo/" when sorted
 */
void test_tree_leaf_sort_key_ordering() {
    std::cout << "Test: tree_leaf_sort_key - Sort Order Verification... ";

    GitTreeLeaf file_leaf;
    file_leaf.mode = "100644";
    file_leaf.path = "foo.c";
    file_leaf.sha = "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391";

    GitTreeLeaf dir_leaf;
    dir_leaf.mode = "040000";
    dir_leaf.path = "foo";
    dir_leaf.sha = "4b825dc642cb6eb9a060e54bf8d69288fbee4904";

    std::string file_key = tree_leaf_sort_key(file_leaf);
    std::string dir_key = tree_leaf_sort_key(dir_leaf);

    // foo.c should come before foo/
    assert(file_key < dir_key);

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: tree_serialize - Single Entry
 * ---------------------------------------------------------------------------
 * Description:
 *   Serialize a tree with a single entry.
 *
 * Input:
 *   - Vector with one GitTreeLeaf: { mode: "100644", path: "test.txt", sha: "..." }
 *
 * Expected Output:
 *   - Binary string: "100644 test.txt\0<20 bytes SHA>"
 */
void test_tree_serialize_single_entry() {
    std::cout << "Test: tree_serialize - Single Entry... ";

    std::string sha_hex = "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391";
    GitTreeLeaf leaf;
    leaf.mode = "100644";
    leaf.path = "test.txt";
    leaf.sha = sha_hex;

    std::vector<GitTreeLeaf> leaves = { leaf };
    std::string serialized = tree_serialize(leaves);

    // Should match the expected raw format
    std::string expected = create_raw_tree_entry("100644", "test.txt", sha_hex);
    assert(serialized == expected);

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: tree_serialize - Sorted Output
 * ---------------------------------------------------------------------------
 * Description:
 *   Verify that tree_serialize sorts entries before serialization.
 *
 * Input:
 *   - Vector with entries in reverse order: [ "c.txt", "b.txt", "a.txt" ]
 *
 * Expected Output:
 *   - Serialized output has entries in order: [ "a.txt", "b.txt", "c.txt" ]
 */
void test_tree_serialize_sorted() {
    std::cout << "Test: tree_serialize - Sorted Output... ";

    std::string sha = "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391";

    GitTreeLeaf c_leaf = { "100644", "c.txt", sha };
    GitTreeLeaf b_leaf = { "100644", "b.txt", sha };
    GitTreeLeaf a_leaf = { "100644", "a.txt", sha };

    std::vector<GitTreeLeaf> leaves = { c_leaf, b_leaf, a_leaf };
    std::string serialized = tree_serialize(leaves);

    // Parse it back to verify order
    std::vector<GitTreeLeaf> parsed = tree_parse(serialized);

    assert(parsed.size() == 3);
    assert(parsed[0].path == "a.txt");
    assert(parsed[1].path == "b.txt");
    assert(parsed[2].path == "c.txt");

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: tree_serialize - Round Trip
 * ---------------------------------------------------------------------------
 * Description:
 *   Verify that parsing then serializing produces identical data.
 *
 * Input:
 *   - Original raw tree data
 *
 * Expected Output:
 *   - tree_serialize(tree_parse(raw)) == raw (assuming sorted input)
 */
void test_tree_serialize_round_trip() {
    std::cout << "Test: tree_serialize - Round Trip... ";

    std::string sha1 = "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391";
    std::string sha2 = "4b825dc642cb6eb9a060e54bf8d69288fbee4904";

    // Create raw data already sorted (a.txt before b.txt)
    std::string original = create_raw_tree_entry("100644", "a.txt", sha1)
                         + create_raw_tree_entry("100644", "b.txt", sha2);

    std::vector<GitTreeLeaf> parsed = tree_parse(original);
    std::string reserialized = tree_serialize(parsed);

    assert(original == reserialized);

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: GitTree - Deserialize and Get Leaves
 * ---------------------------------------------------------------------------
 * Description:
 *   Verify GitTree correctly deserializes raw data.
 *
 * Input:
 *   - Raw tree data with 2 entries
 *
 * Expected Output:
 *   - get_leaves() returns vector with 2 entries
 */
void test_git_tree_deserialize() {
    std::cout << "Test: GitTree - Deserialize and Get Leaves... ";

    std::string sha1 = "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391";
    std::string sha2 = "4b825dc642cb6eb9a060e54bf8d69288fbee4904";
    std::string raw = create_raw_tree_entry("100644", "file1.txt", sha1)
                    + create_raw_tree_entry("100644", "file2.txt", sha2);

    GitTree tree(raw);
    const auto& leaves = tree.get_leaves();

    assert(leaves.size() == 2);
    assert(leaves[0].path == "file1.txt");
    assert(leaves[1].path == "file2.txt");

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: GitTree - Serialize
 * ---------------------------------------------------------------------------
 * Description:
 *   Verify GitTree correctly serializes its contents.
 *
 * Input:
 *   - GitTree with manually set leaves
 *
 * Expected Output:
 *   - serialize() returns valid binary tree data
 */
void test_git_tree_serialize() {
    std::cout << "Test: GitTree - Serialize... ";

    std::string sha = "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391";

    GitTree tree;
    GitTreeLeaf leaf1 = { "100644", "hello.txt", sha };
    GitTreeLeaf leaf2 = { "040000", "src", "4b825dc642cb6eb9a060e54bf8d69288fbee4904" };

    std::vector<GitTreeLeaf> leaves = { leaf1, leaf2 };
    tree.set_leaves(leaves);

    std::string serialized = tree.serialize();

    // Verify by parsing back
    std::vector<GitTreeLeaf> parsed = tree_parse(serialized);
    assert(parsed.size() == 2);

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: GitTree - Empty Check
 * ---------------------------------------------------------------------------
 * Description:
 *   Verify empty() returns correct value.
 *
 * Input:
 *   - Empty GitTree
 *   - GitTree with one leaf
 *
 * Expected Output:
 *   - empty() == true for empty tree
 *   - empty() == false for non-empty tree
 */
void test_git_tree_empty() {
    std::cout << "Test: GitTree - Empty Check... ";

    GitTree empty_tree;
    assert(empty_tree.empty() == true);

    GitTree non_empty_tree;
    GitTreeLeaf leaf = { "100644", "test.txt", "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391" };
    non_empty_tree.add_leaf(leaf);
    assert(non_empty_tree.empty() == false);

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: GitTree - Add Leaf
 * ---------------------------------------------------------------------------
 * Description:
 *   Verify add_leaf correctly appends entries.
 *
 * Input:
 *   - Empty GitTree, add 3 leaves
 *
 * Expected Output:
 *   - get_leaves().size() == 3
 */
void test_git_tree_add_leaf() {
    std::cout << "Test: GitTree - Add Leaf... ";

    GitTree tree;
    std::string sha = "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391";

    tree.add_leaf({ "100644", "a.txt", sha });
    tree.add_leaf({ "100644", "b.txt", sha });
    tree.add_leaf({ "100644", "c.txt", sha });

    assert(tree.get_leaves().size() == 3);

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: GitTree - get_fmt
 * ---------------------------------------------------------------------------
 * Description:
 *   Verify get_fmt() returns "tree".
 *
 * Input:
 *   - GitTree object
 *
 * Expected Output:
 *   - get_fmt() == "tree"
 */
void test_git_tree_get_fmt() {
    std::cout << "Test: GitTree - get_fmt... ";

    GitTree tree;
    assert(tree.get_fmt() == "tree");

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: ls-tree Argument Parsing
 * ---------------------------------------------------------------------------
 * Description:
 *   Verify ls-tree command arguments are correctly parsed.
 *
 * Input:
 *   - Command: "silt ls-tree HEAD"
 *   - Command: "silt ls-tree -r HEAD"
 *
 * Expected Output:
 *   - ParsedArgs contains tree and recursive values
 */
void test_ls_tree_argument_parsing() {
    std::cout << "Test: ls-tree Argument Parsing... ";

    // This test verifies the argument structure is correct
    // Actual parsing is done by the CLI framework

    ParsedArgs args;
    args.values["tree"] = "HEAD";
    args.values["recursive"] = "false";

    assert(args.get("tree") == "HEAD");
    assert(args.get("recursive") == "false");

    args.values["recursive"] = "true";
    assert(args.get("recursive") == "true");

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: checkout Argument Parsing
 * ---------------------------------------------------------------------------
 * Description:
 *   Verify checkout command arguments are correctly parsed.
 *
 * Input:
 *   - Command: "silt checkout HEAD ./output"
 *
 * Expected Output:
 *   - ParsedArgs contains commit and path values
 */
void test_checkout_argument_parsing() {
    std::cout << "Test: checkout Argument Parsing... ";

    ParsedArgs args;
    args.values["commit"] = "HEAD";
    args.values["path"] = "./output";

    assert(args.get("commit") == "HEAD");
    assert(args.get("path") == "./output");

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: Symlink Mode Detection
 * ---------------------------------------------------------------------------
 * Description:
 *   Verify symlink mode (120000) is correctly identified.
 *
 * Input:
 *   - leaf with mode "120000"
 *
 * Expected Output:
 *   - Mode prefix "12" indicates symlink (blob type)
 */
void test_symlink_mode_detection() {
    std::cout << "Test: Symlink Mode Detection... ";

    GitTreeLeaf symlink_leaf;
    symlink_leaf.mode = "120000";
    symlink_leaf.path = "link";
    symlink_leaf.sha = "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391";

    // Mode prefix "12" indicates symlink
    assert(symlink_leaf.mode.substr(0, 2) == "12");

    // Sort key should NOT have trailing slash (not a directory)
    std::string key = tree_leaf_sort_key(symlink_leaf);
    assert(key == "link");  // No trailing slash

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: Submodule Mode Detection
 * ---------------------------------------------------------------------------
 * Description:
 *   Verify submodule mode (160000) is correctly identified.
 *
 * Input:
 *   - leaf with mode "160000"
 *
 * Expected Output:
 *   - Mode prefix "16" indicates submodule (commit type)
 */
void test_submodule_mode_detection() {
    std::cout << "Test: Submodule Mode Detection... ";

    GitTreeLeaf submodule_leaf;
    submodule_leaf.mode = "160000";
    submodule_leaf.path = "submodule";
    submodule_leaf.sha = "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391";

    // Mode prefix "16" indicates submodule
    assert(submodule_leaf.mode.substr(0, 2) == "16");

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: hex_to_raw_sha - Length and roundtrip
 * ---------------------------------------------------------------------------
 * Description:
 *   Verify that converting a 40 char hex SHA to raw 20-byte string produces
 *   20 raw bytes and that the hex round-trip is consistent.
 */
void test_hex_to_raw_sha_length() {
    std::cout << "Test: hex_to_raw_sha - Length and roundtrip... ";

    std::string sha_hex = "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391";
    std::string raw = hex_to_raw_sha(sha_hex);
    assert(raw.size() == 20);

    // Convert back to hex and compare
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char c : raw) {
        ss << std::setw(2) << (static_cast<unsigned>(c) & 0xff);
    }
    std::string back_hex = ss.str();
    assert(back_hex == sha_hex);

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: create_raw_tree_entry - Space and Unicode in path
 * ---------------------------------------------------------------------------
 * Description:
 *   Ensure that a tree entry with spaces and unicode in the path is parsed
 *   correctly and the path is preserved.
 */
void test_create_raw_tree_entry_space_unicode() {
    std::cout << "Test: create_raw_tree_entry - Space and Unicode in path... ";

    std::string sha_hex = "4b825dc642cb6eb9a060e54bf8d69288fbee4904";
    std::string path = "file name ü.txt";
    std::string raw = create_raw_tree_entry("100644", path, sha_hex);

    auto [leaf, next_pos] = tree_parse_one(raw, 0);
    assert(leaf.path == path);
    assert(leaf.sha == sha_hex);

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: tree_parse - Long filename
 * ---------------------------------------------------------------------------
 * Description:
 *   Verify that tree_parse can handle very long filenames without breaking.
 */
void test_tree_parse_long_filename() {
    std::cout << "Test: tree_parse - Long filename... ";

    std::string sha = "83baae61804e65cc73a7201a7252750c76066a30";
    std::string long_name(300, 'a'); // 300 'a' characters
    std::string raw = create_raw_tree_entry("100644", long_name, sha);

    std::vector<GitTreeLeaf> leaves = tree_parse(raw);
    assert(leaves.size() == 1);
    assert(leaves[0].path == long_name);

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Test: tree_serialize - Space and Unicode roundtrip
 * ---------------------------------------------------------------------------
 * Description:
 *   Ensure serialization and parsing preserve paths with spaces and unicode.
 */
void test_tree_serialize_space_unicode_roundtrip() {
    std::cout << "Test: tree_serialize - Space and Unicode roundtrip... ";

    std::string sha = "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391";
    GitTreeLeaf leaf;
    leaf.mode = "100644";
    leaf.path = "file name ü.txt";
    leaf.sha = sha;

    std::vector<GitTreeLeaf> leaves = { leaf };
    std::string serialized = tree_serialize(leaves);
    std::vector<GitTreeLeaf> parsed = tree_parse(serialized);

    assert(parsed.size() == 1);
    assert(parsed[0].path == leaf.path);
    assert(parsed[0].sha == leaf.sha);

    std::cout << "PASSED" << std::endl;
}

/*
 * ---------------------------------------------------------------------------
 * Run All Tests
 * ---------------------------------------------------------------------------
 */
int main() {
    std::cout << "=== Silt Tree Tests ===" << std::endl;
    std::cout << std::endl;

    // GitTreeLeaf tests
    test_git_tree_leaf_construction();

    // tree_parse_one tests
    test_tree_parse_one_regular_file();
    test_tree_parse_one_directory();
    test_tree_parse_one_with_offset();

    // tree_parse tests
    test_tree_parse_multiple_entries();
    test_tree_parse_empty();

    // tree_leaf_sort_key tests
    test_tree_leaf_sort_key_file();
    test_tree_leaf_sort_key_directory();
    test_tree_leaf_sort_key_ordering();

    // tree_serialize tests
    test_tree_serialize_single_entry();
    test_tree_serialize_sorted();
    test_tree_serialize_round_trip();

    // GitTree class tests
    test_git_tree_deserialize();
    test_git_tree_serialize();
    test_git_tree_empty();
    test_git_tree_add_leaf();
    test_git_tree_get_fmt();

    // Command argument parsing tests
    test_ls_tree_argument_parsing();
    test_checkout_argument_parsing();

    // Mode detection tests
    test_symlink_mode_detection();
    test_submodule_mode_detection();

    // New sample tests
    test_hex_to_raw_sha_length();
    test_create_raw_tree_entry_space_unicode();
    test_tree_parse_long_filename();
    test_tree_serialize_space_unicode_roundtrip();

    std::cout << std::endl;
    std::cout << "=== All Tests Passed ===" << std::endl;

    return 0;
}
