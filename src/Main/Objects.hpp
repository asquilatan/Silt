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

class GitTreeLeaf {
public:
    std::string mode;
    std::string path;
    std::string sha;

    GitTreeLeaf(std::string mode, std::string path, std::string sha)
        : mode(mode), path(path), sha(sha) {}


};

std::pair<GitTreeLeaf, size_t> tree_parse_one(const std::string& raw, size_t start = 0);
std::vector<GitTreeLeaf> tree_parse(const std::string& raw);
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

    virtual void initialize() { }
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
    GitCommit() = default;
    GitCommit(const std::string& data) {
        deserialize(data);
    }

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

    const KVLM& get_kvlm() const {
        return kvlm;
    }
private:
    KVLM kvlm;
};

class GitTree : public GitObject {
private:
    std::vector<GitTreeLeaf> leaves;
public:
    // Constructor for creating a new object with data
    GitTree() = default;
    GitTree(const std::string& data) {
        deserialize(data);
    }

    std::string serialize() override;
    void deserialize(const std::string& data) override;

    std::string get_fmt() const override {
        return "tree";
    }

    // Accessors for tree leaves
    const std::vector<GitTreeLeaf>& get_leaves() const;
    void set_leaves(const std::vector<GitTreeLeaf>& new_leaves);
    void add_leaf(const GitTreeLeaf& leaf);
    bool empty() const;

    // init
    void initialize();
};

class GitTag : public GitCommit {
public:
    GitTag() = default;
    GitTag(const std::string& data) : GitCommit(data) {}

    // return format type
    std::string get_fmt() const override {
        return "tag";
    }
};

std::string object_write(std::unique_ptr<GitObject> obj, Repository* repo = nullptr);

std::optional<std::unique_ptr<GitObject>> object_read(Repository* repo, char* sha);

std::vector<std::string> object_resolve(Repository* repo, std::string name);

std::string object_find(Repository* repo, std::string name, std::string fmt, bool follow=true);

std::string object_hash(const std::string& data, const std::string& fmt, Repository* repo = nullptr);