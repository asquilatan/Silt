#include <stdexcept>
#include <cstddef>
#include <optional>
#include <string> // Added for std::string
#include <memory> // Added for std::unique_ptr
#include "Repository.hpp" // Added for Repository class

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
    // TODO: Implement commit-specific serialize/deserialize

    // return format type
    std::string get_fmt() const override {
        return "commit";
    }
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