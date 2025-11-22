#include <stdexcept>
#include <cstddef>
#include <optional>
#include <string> // Added for std::string
#include <memory> // Added for std::unique_ptr
#include <map>    // Added for std::map used in kvlm functions
#include <variant> // Added for std::variant
#include "Repository.hpp" // Added for Repository class

// key-value list with message (KVLM) functions
using KVLMValue = std::variant<std::string, std::vector<std::string>>;
using KVLM = std::map<std::string, KVLMValue>;

KVLM kvlm_parse(const std::string& data);
KVLM kvlm_parse(const std::string& data, int start, KVLM dct);
std::string kvlm_serialize(const KVLM& kvlm);

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
    using GitObject::GitObject;

    // return format type
    std::string get_fmt() const override {
        return "tree";
    }
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