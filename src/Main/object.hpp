#include <stdexcept>
#include <cstddef>
#include <optional>
#include <string> // Added for std::string

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
};