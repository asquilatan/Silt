#include <stdexcept>
#include <cstddef>
#include <stdexcept>
#include <optional>
#include "Repository.hpp"
#include <cstring>
#include <zlib.h>
#include <fstream>
#include <memory>
#include <vector>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>

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
    GitBlob(const std::string& data) {
        blobdata = data;
    }

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

std::optional<std::unique_ptr<GitObject>> object_read(Repository* repo, char* sha) {
    // first 2 digits of sha
    char* dirname = new char[3];
    strncpy(dirname, sha, 2);
    dirname[2] = '\0';

    // next few digits of sha
    char* filename = new char[strlen(sha) - 1];
    strncpy(filename, sha + 2, strlen(sha) - 2);
    filename[strlen(sha) - 2] = '\0';

    std::filesystem::path path = repo_file(*repo, "objects", dirname, filename);

    // if path is not a file
    if (!std::filesystem::is_regular_file(path)) {
        return std::nullopt;
    }

    // delete these because they're not needed anymore
    delete[] dirname;
    delete[] filename;

    // read file from path as binary
    std::ifstream file(path, std::ios::binary);

    // store the data in a vector
    std::vector<char> compressed_data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // close the file
    file.close();

    // creates a var of type z_stream
    z_stream zs;

    // initializes the bytes of zs to 0
    memset(&zs, 0, sizeof(zs));

    // basically, Z_OK is an integer, it's returned when zs is properly inflated via inflateInit
    // if it isn't Z_OK, then something went wrong, so throw a runtime err
    if (inflateInit(&zs) != Z_OK) {
        throw std::runtime_error("Failed to initialize zlib inflation.");
    }

    // tell z_stream where the inp data is, .data() just points to the first element of the vector
    zs.next_in = (Bytef*)compressed_data.data();

    // size of the vector
    zs.avail_in = compressed_data.size();

    // ret is the status of inflation
    // outbuffer is basically a buffer for the output of the inflation
    // decompressed_data contains the final decompressed data
    int ret;
    char outbuffer[32768];
    std::string decompressed_data;

    do {
        // next_out points to where inflate should write the chunk of decompressed data
        // reinterpret_cast converts outbuffer from char* to bytef*
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);

        // available output space
        zs.avail_out = sizeof(outbuffer);

        // inflate zs, assign ret to the return code of inflate (Z_OK is good, anything else is bad)
        ret = inflate(&zs, Z_NO_FLUSH);

        // if the decompressed data size is less than the output (if inflate wrote any new decompressed bytes)
        // append the remaining chunks to decompressed_data
        if (decompressed_data.size() < zs.total_out) {
            decompressed_data.append(outbuffer, zs.total_out - decompressed_data.size());
        }

    } while (ret == Z_OK);

    // end the inflation
    inflateEnd(&zs);

    // if the return value is not Z_STREAM_END, throw runtime err
    if (ret != Z_STREAM_END) {
        throw std::runtime_error("Zlib inflation failed: " + std::string(zs.msg));
    }

    // find a space
    auto space_pos = decompressed_data.find(' ');
    // if the space position does not exist, throw runtime err
    if (space_pos == std::string::npos) {
        throw std::runtime_error("Invalid object format: missing space.");
    }

    // find a null terminator
    auto null_pos = decompressed_data.find('\0', space_pos);
    // if it doesn't exist, throw runtime err
    if (null_pos == std::string::npos) {
        throw std::runtime_error("Invalid object format: missing null terminator.");
    }

    // take the format, starting from 0 to space position (exclusive)
    // `commit 1028/0tree` would give `commit`
    std::string fmt = decompressed_data.substr(0, space_pos);

    // take the size of the object (numbers before the null terminator)
    // `commit 1028/0tree` would give `1028`
    size_t size = std::stoul(decompressed_data.substr(space_pos + 1, null_pos - (space_pos + 1)));

    // take the content of the object (everything after the null terminator)
    std::string content = decompressed_data.substr(null_pos + 1);

    // if the content length and size is not the same, throw runtime err
    if (size != content.length()) {
        throw std::runtime_error("Malformed object: size mismatch.");
    }

    // pick constructor based on object type (fmt)
    if (fmt == "commit") {
        // convert unique_ptr<GitCommit> to unique_ptr<GitObject>
        std::unique_ptr<GitObject> obj = std::make_unique<GitCommit>(content);
        return obj;
    } else if (fmt == "tree") {
        // convert unique_ptr<GitTree> to unique_ptr<GitObject>
        std::unique_ptr<GitObject> obj = std::make_unique<GitTree>(content);
        return obj;
    } else if (fmt == "tag") {
        // convert unique_ptr<GitTag> to unique_ptr<GitObject>
        std::unique_ptr<GitObject> obj = std::make_unique<GitTag>(content);
        return obj;
    } else if (fmt == "blob") {
        // convert unique_ptr<GitBlob> to unique_ptr<GitObject>
        std::unique_ptr<GitObject> obj = std::make_unique<GitBlob>(content);
        return obj;
    } else {
        throw std::runtime_error("Unknown type for object " + std::string(sha));
    }
}

std::string object_write(std::unique_ptr<GitObject> obj, Repository* repo) {
    // Serialize object data
    std::string data = obj->serialize();
    // Add header (format + space + size + null terminator + data)
    std::string result = obj->get_fmt() + " " + std::to_string(data.length()) + '\0' + data;

    // Compute SHA1 hash
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(result.c_str()), result.length(), hash);

    // Convert hash to hex string
    std::stringstream sha_stream;
    sha_stream << std::hex << std::setfill('0');
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        sha_stream << std::setw(2) << static_cast<unsigned>(hash[i]);
    }
    std::string sha = sha_stream.str();

    if (repo) {
        // Compute path
        char* sha_ptr = const_cast<char*>(sha.c_str());
        char* dirname = new char[3];
        strncpy(dirname, sha_ptr, 2);
        dirname[2] = '\0';

        char* filename = new char[sha.length() - 1];
        strncpy(filename, sha_ptr + 2, sha.length() - 2);
        filename[sha.length() - 2] = '\0';

        std::filesystem::path path = repo_file(*repo, "objects", dirname, filename);

        // Create parent directories if they don't exist
        std::filesystem::create_directories(path.parent_path());

        // Check if the file doesn't already exist
        if (!std::filesystem::exists(path)) {
            // Compress the result
            z_stream zs;
            memset(&zs, 0, sizeof(zs));

            if (deflateInit(&zs, Z_DEFAULT_COMPRESSION) != Z_OK) {
                throw std::runtime_error("Failed to initialize zlib compression.");
            }

            zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(result.c_str()));
            zs.avail_in = result.length();

            int ret;
            std::string compressed_data;
            char outbuffer[32768];

            do {
                zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
                zs.avail_out = sizeof(outbuffer);

                ret = deflate(&zs, Z_SYNC_FLUSH);

                if (compressed_data.size() < zs.total_out) {
                    compressed_data.append(outbuffer, zs.total_out - compressed_data.size());
                }
            } while (ret == Z_OK);

            deflateEnd(&zs);

            // Write the compressed data to the file
            std::ofstream file(path, std::ios::binary);
            file.write(compressed_data.c_str(), compressed_data.length());
            file.close();
        }

        // Clean up allocated memory
        delete[] dirname;
        delete[] filename;
    }

    return sha;
}

std::string object_find(Repository* repo, std::string name, std::string fmt, bool follow=true) {
    return name;
}

std::string object_hash(const std::string& data, const std::string& fmt, Repository* repo = nullptr) {
    std::unique_ptr<GitObject> obj;

    if (fmt == "commit") {
        obj = std::make_unique<GitCommit>(data);
    } else if (fmt == "tree") {
        obj = std::make_unique<GitTree>(data);
    } else if (fmt == "tag") {
        obj = std::make_unique<GitTag>(data);
    } else if (fmt == "blob") {
        obj = std::make_unique<GitBlob>(data);
    } else {
        throw std::runtime_error("Unknown type: " + fmt);
    }

    return object_write(std::move(obj), repo);
}