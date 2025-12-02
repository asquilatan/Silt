#pragma once
#include <stdexcept>
#include <cstddef>
#include <optional>
#include <string> // Added for std::string
#include <memory> // Added for std::unique_ptr
#include <map>    // Added for std::map used in kvlm functions
#include <variant> // Added for std::variant
#include <vector>
#include <utility>
#include "Repository.hpp" // Added for Repository class

// key-value list with message (KVLM) functions
using KVLMValue = std::variant<std::string, std::vector<std::string>>;
using KVLM = std::map<std::string, KVLMValue>;

KVLM kvlm_parse(const std::string& data);
KVLM kvlm_parse(const std::string& data, int start, KVLM dct);
std::string kvlm_serialize(const KVLM& kvlm);

/*
 * Problem: GitTreeLeaf
 * ---------------------------------------------------------------------------
 * Description:
 *   Design a structure to represent a single entry within a Git tree object.
 *   A tree entry contains metadata about a file or subdirectory: its mode
 *   (permissions and type), its path (name), and the SHA-1 hash pointing to
 *   the corresponding blob or subtree object.
 *
 * Input:
 *   - mode: A string representing the file mode (e.g., "100644" for regular
 *           file, "040000" for directory, "120000" for symlink).
 *   - path: A string representing the name of the file or directory.
 *   - sha:  A 40-character hexadecimal string representing the SHA-1 hash
 *           of the blob or tree object this entry points to.
 *
 * Output:
 *   - A GitTreeLeaf object that bundles mode, path, and sha together.
 *
 * Example:
 *   Input:  mode = "100644", path = "README.md",
 *           sha = "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391"
 *   Output: GitTreeLeaf { mode: "100644", path: "README.md",
 *                         sha: "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391" }
 *
 * Constraints:
 *   - mode is 5 or 6 ASCII digits (normalize 5-digit modes to 6 by prepending '0')
 *   - path contains no null bytes
 *   - sha is exactly 40 lowercase hexadecimal characters
 */
class GitTreeLeaf {
public:
    std::string mode;
    std::string path;
    std::string sha;

    GitTreeLeaf(std::string mode, std::string path, std::string sha)
        : mode(mode), path(path), sha(sha) {}


};

/*
 * Problem: tree_parse_one
 * ---------------------------------------------------------------------------
 * Description:
 *   Parse a single tree entry from a raw Git tree object payload. Tree entries
 *   are stored in binary format: [mode] [space] [path] [null] [20-byte SHA].
 *   This function extracts one entry starting at the given offset and returns
 *   both the parsed leaf and the position to continue parsing from.
 *
 * Input:
 *   - raw:   A string containing the raw tree object data (may include null bytes).
 *   - start: The offset within raw to begin parsing from (default: 0).
 *
 * Output:
 *   - A pair containing:
 *     1. GitTreeLeaf: The parsed tree entry with mode, path, and sha fields.
 *     2. size_t: The offset immediately after this entry (for parsing the next one).
 *
 * Example:
 *   Input:  raw = "100644 file.txt\0<20 bytes of SHA>...", start = 0
 *   Output: { GitTreeLeaf{mode:"100644", path:"file.txt", sha:"<40-char hex>"}, 28 }
 *
 * Constraints:
 *   - raw must contain a valid tree entry starting at position start
 *   - Mode is 5 or 6 bytes followed by a space (0x20)
 *   - Path is null-terminated (0x00)
 *   - SHA is 20 raw bytes which must be converted to 40-char hex string
 */
std::pair<GitTreeLeaf, size_t> tree_parse_one(const std::string& raw, size_t start = 0);

/*
 * Problem: tree_parse
 * ---------------------------------------------------------------------------
 * Description:
 *   Parse an entire Git tree object payload into a list of tree entries.
 *   A tree object is simply the concatenation of multiple entries, each in
 *   the format parsed by tree_parse_one. This function iterates through the
 *   raw data until all entries have been extracted.
 *
 * Input:
 *   - raw: A string containing the complete raw tree object data.
 *
 * Output:
 *   - A vector of GitTreeLeaf objects, one for each entry in the tree.
 *
 * Example:
 *   Input:  raw = "<entry1><entry2><entry3>" (binary concatenation)
 *   Output: [ GitTreeLeaf{...}, GitTreeLeaf{...}, GitTreeLeaf{...} ]
 *
 * Constraints:
 *   - raw must be a valid Git tree object payload
 *   - The function should handle empty trees (raw.empty() returns empty vector)
 */
std::vector<GitTreeLeaf> tree_parse(const std::string& raw);

/*
 * Problem: tree_leaf_sort_key
 * ---------------------------------------------------------------------------
 * Description:
 *   Generate a sort key for a GitTreeLeaf to ensure trees are serialized in
 *   Git's canonical order. Git sorts tree entries by name, but directories
 *   (trees) are sorted as if they had a trailing slash. This ensures that
 *   "foo" (directory) sorts after "foo.c" (file) rather than before.
 *
 * Input:
 *   - leaf: A GitTreeLeaf object to generate a sort key for.
 *
 * Output:
 *   - A string that can be used as a sort key. For files, return the path.
 *     For directories (mode starts with "04"), return path + "/".
 *
 * Example:
 *   Input:  leaf = { mode: "100644", path: "foo.c", sha: "..." }
 *   Output: "foo.c"
 *
 *   Input:  leaf = { mode: "040000", path: "foo", sha: "..." }
 *   Output: "foo/"
 *
 * Constraints:
 *   - Directory entries have mode starting with "04" (e.g., "040000")
 *   - File entries have mode starting with "10" (regular), "12" (symlink), etc.
 */
std::string tree_leaf_sort_key(const GitTreeLeaf& leaf);

/*
 * Problem: tree_serialize
 * ---------------------------------------------------------------------------
 * Description:
 *   Serialize a vector of GitTreeLeaf entries back into the raw binary format
 *   used by Git tree objects. Before serialization, entries must be sorted
 *   using tree_leaf_sort_key to maintain Git's canonical ordering.
 *
 * Input:
 *   - leaves: A vector of GitTreeLeaf entries (order may be arbitrary).
 *
 * Output:
 *   - A string containing the serialized tree data in binary format:
 *     [mode][space][path][null][20-byte SHA] for each entry, concatenated.
 *
 * Example:
 *   Input:  [ { mode: "100644", path: "b.txt", sha: "abc..." },
 *             { mode: "100644", path: "a.txt", sha: "def..." } ]
 *   Output: "<sorted binary data with a.txt before b.txt>"
 *
 * Constraints:
 *   - SHA must be converted from 40-char hex to 20 raw bytes
 *   - Entries must be sorted before serialization
 *   - Mode should not have leading zeros stripped (use as-is)
 */
std::string tree_serialize(std::vector<GitTreeLeaf> leaves);

class GitObject {
public:
    // Constructor for creating a new object
    GitObject() = default;

    // Constructor for deserializing from existing data
    GitObject(const std::string& data) {
        deserialize(data);
    }

    // Virtual destructor for proper cleanup of derived classes
    virtual ~GitObject() = default;

    virtual std::string serialize() {
        throw std::runtime_error("Serialize method not implemented for base GitObject.");
    }

    virtual void deserialize(const std::string& data) {
        throw std::runtime_error("Deserialize method not implemented for base GitObject.");
    }

    virtual std::string get_fmt() const {
        throw std::runtime_error("Get_fmt method not implemented for base GitObject.");
    }

    virtual void initialize(const std::string& data) {

    }
};

class GitBlob : public GitObject {
public:
    // Constructor for creating a new object with data
    GitBlob(const std::string& data);

    // Default constructor
    GitBlob() = default;

    // overrides serialize, returns blob data
    std::string serialize() override {
        return blobdata;
    }

    // overrides deserialize, stores data as blobdata
    void deserialize(const std::string& data) override {
        blobdata = data;
    }

    // return format type
    std::string get_fmt() const override {
        return "blob";
    }
private:
    std::string blobdata;
};

// Define other GitObject subclasses
class GitCommit : public GitObject {
public:
    using GitObject::GitObject;

    // overrides serialize, returns blob data
    std::string serialize() override {
        return kvlm_serialize(kvlm);
    }

    // overrides deserialize, stores data as blobdata
    void deserialize(const std::string& data) override {
        kvlm = kvlm_parse(data);
    }

    // return format type
    std::string get_fmt() const override {
        return "commit";
    }
private:
    KVLM kvlm;
};

class GitTree : public GitObject {
public:
    /*
     * Problem: GitTree
     * -------------------------------------------------------------------------
     * Description:
     *   Implement a GitTree class that represents a Git tree object. A tree
     *   object maps names to blobs (files) and other trees (subdirectories).
     *   The class must support deserialization from raw Git object data and
     *   serialization back to the canonical binary format.
     *
     * Input (for deserialization):
     *   - data: Raw tree object payload as stored in Git's object database.
     *
     * Output (for serialization):
     *   - A binary string in Git's tree format, properly sorted.
     *
     * Example:
     *   Input:  Raw tree data containing entries for "README.md" and "src/"
     *   Output: Serialized tree with entries in lexicographic order
     *
     * Methods Required:
     *   - serialize():    Convert internal leaves to raw binary format
     *   - deserialize():  Parse raw binary data into internal leaves
     *   - get_leaves():   Return const reference to parsed entries
     *   - set_leaves():   Replace all entries
     *   - add_leaf():     Append a single entry
     *   - empty():        Check if tree has no entries
     *
     * Constraints:
     *   - Must maintain entries in sorted order after serialization
     *   - get_fmt() must return "tree"
     */
    GitTree() = default;
    using GitObject::GitObject;

    std::string serialize() override;
    void deserialize(const std::string& data) override;
    std::string get_fmt() const override {
        return "tree";
    }

    const std::vector<GitTreeLeaf>& get_leaves() const;
    void set_leaves(const std::vector<GitTreeLeaf>& new_leaves);
    void add_leaf(const GitTreeLeaf& leaf);
    bool empty() const;

private:
    std::vector<GitTreeLeaf> leaves;
};

class GitTag : public GitObject {
public:
    using GitObject::GitObject;

    // return format type
    std::string get_fmt() const override {
        return "tag";
    }
};

std::string object_write(std::unique_ptr<GitObject> obj, Repository* repo = nullptr);

std::optional<std::unique_ptr<GitObject>> object_read(Repository* repo, char* sha);

std::string object_find(Repository* repo, std::string name, std::string fmt, bool follow=true);

std::string object_hash(const std::string& data, const std::string& fmt, Repository* repo = nullptr);