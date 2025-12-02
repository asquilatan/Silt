#include <stdexcept>
#include <cstddef>
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
#include <map>
#include <variant>
#include <iterator>
#include <algorithm>
#include "Objects.hpp"  // Include the header file to get KVLM types

// Implement the GitBlob constructor that takes a string
GitBlob::GitBlob(const std::string& data) {
    deserialize(data);
}

std::string object_write(std::unique_ptr<GitObject> obj, Repository* repo);

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

std::string object_find(Repository* repo, std::string name, std::string fmt, bool follow) {
    // If name is empty, return it as is
    if (name.empty()) {
        return name;
    }

    // If name is "HEAD", resolve it to the actual commit SHA
    if (name == "HEAD") {
        // Read the .git/HEAD file to get the reference
        std::filesystem::path head_path = repo->gitdir / "HEAD";

        if (std::filesystem::exists(head_path)) {
            std::ifstream head_file(head_path);
            std::string head_content((std::istreambuf_iterator<char>(head_file)),
                                   std::istreambuf_iterator<char>());

            // Remove any trailing newlines
            if (!head_content.empty() && head_content.back() == '\n') {
                head_content.pop_back();
            }

            // HEAD usually contains something like "ref: refs/heads/main"
            if (head_content.substr(0, 5) == "ref: ") {
                // Extract the reference part
                std::string ref = head_content.substr(5); // "refs/heads/main"

                // Read the reference file to get the commit SHA
                std::filesystem::path ref_path = repo->gitdir / ref;

                if (std::filesystem::exists(ref_path)) {
                    std::ifstream ref_file(ref_path);
                    std::string sha((std::istreambuf_iterator<char>(ref_file)),
                                  std::istreambuf_iterator<char>());

                    // Remove any trailing newlines
                    if (!sha.empty() && sha.back() == '\n') {
                        sha.pop_back();
                    }

                    return sha;
                } else {
                    throw std::runtime_error("Reference file does not exist: " + ref);
                }
            } else {
                // If HEAD doesn't start with "ref: ", it might contain a SHA directly
                return head_content;
            }
        } else {
            throw std::runtime_error("HEAD file does not exist");
        }
    }

    return name;
}

std::string object_hash(const std::string& data, const std::string& fmt, Repository* repo) {
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

KVLM kvlm_parse(const std::string& data) {
    return kvlm_parse(data, 0, KVLM{});
}

KVLM kvlm_parse(const std::string& data, int start, KVLM dct) {
    // find space starting from start, assign to space
    size_t space = data.find(' ', start);
    // find newline, assign to newline
    size_t newline = data.find('\n', start);

    // BASE CASE
    // if space is less than 0, or newline is less than space
    if (space == std::string::npos || newline < space) {
        if (newline != start) {
            throw std::runtime_error("Malformed commit/tag: expected blank line");
        }
        // dict at None? = data[start+1 up to the end]
        std::string message = data.substr(start + 1);
        dct[""] = message;
        return dct;
    }

    // RECURSIVE CASE
    // let the key be the data string starting from start to space
    std::string key = data.substr(start, space - start);
    // let the end be the start
    size_t end = start;
    // while true
    while (true) {
        // starting from end + 1, look for the position of a newline, assign to end
        end = data.find('\n', end + 1);
        // if data[end + 1] is not a space, break
        if (end + 1 >= data.length() || data[end + 1] != ' ') {
            break;
        }
    }

    // let value be data[space + 1, up to end]
    std::string value = data.substr(space + 1, end - (space + 1));
    // replace all '\n ' with '\n' in value
    size_t pos = 0;
    while ((pos = value.find("\n ", pos)) != std::string::npos) {
        value.replace(pos, 2, "\n");
        pos += 1; // Move past the replaced newline
    }

    // if the key is in dct
    if (dct.find(key) != dct.end()) {
        // if the value of the key is a vector
        if (std::holds_alternative<std::vector<std::string>>(dct[key])) {
            // then push back the value
            std::vector<std::string> vec = std::get<std::vector<std::string>>(dct[key]);
            vec.push_back(value);
            dct[key] = vec;
        } else {
            // dct[key] = [dct[key], value]
            std::string existing_value = std::get<std::string>(dct[key]);
            std::vector<std::string> vec = {existing_value, value};
            dct[key] = vec;
        }
    } else {
        // dct[key] = value
        dct[key] = value;
    }

    // return kvlm_parse(data, start=end+1, dct=dct)
    return kvlm_parse(data, end + 1, dct);
}

std::string kvlm_serialize(const KVLM& kvlm) {
    std::string ret = "";

    // output fields
    for (const auto& pair : kvlm) {
        std::string key = pair.first;
        KVLMValue val = pair.second;

        // skip the message itself (key == "")
        if (key == "") continue;

        std::vector<std::string> val_list;
        // if the type of val isn't a vector
        if (std::holds_alternative<std::string>(val)) {
            // add element to vector
            val_list.push_back(std::get<std::string>(val));
        } else {
            val_list = std::get<std::vector<std::string>>(val);
        }

        // for every value in the list
        for (const std::string& v : val_list) {
            // replace all '\n' with '\n ' in v
            std::string modified_v = v;
            size_t pos = 0;
            while ((pos = modified_v.find("\n", pos)) != std::string::npos) {
                // don't add space if it's already at the beginning of a new line
                if (pos + 1 < modified_v.length() && modified_v[pos + 1] != ' ') {
                    modified_v.replace(pos, 1, "\n ");
                    pos += 2; // Move past the replaced newline + space
                } else {
                    pos += 1; // Move past the newline
                }
            }
            // add to return
            ret += key + " " + modified_v + "\n";
        }
    }

    // Append message
    auto msg_it = kvlm.find("");
    if (msg_it != kvlm.end()) {
        ret += "\n";
        ret += std::get<std::string>(msg_it->second);
    }

    return ret;
}

// tree_parse_one

static std::string sha1_to_hex(const std::string& raw) {
    // convert raw bytes to hex string
    std::ostringstream oss;
    // set the stream to hex mode with leading zeros
    oss << std::hex << std::setfill('0');
    // for every character in raw
    for (unsigned char c : raw) {
        // output the 2-digit hex representation
        oss << std::setw(2) << static_cast<unsigned int>(c);
    }
    // return the hex string
    return oss.str();
}

std::pair<GitTreeLeaf, size_t> tree_parse_one(const std::string& raw, size_t start = 0) {
    // find space in raw string
    size_t space_pos = raw.find(' ', start);
    
    // extract mode
    std::string mode = raw.substr(start, space_pos - start);

    // if the length of mode is 5, prepend a '0'
    if (mode.length() == 5) {
        mode = "0" + mode;
    }
    
    // find null terminator
    size_t null_pos = raw.find('\0', space_pos);

    // extract path
    std::string path = raw.substr(space_pos + 1, null_pos - space_pos - 1);

    // extract sha, convert from raw bytes to hex
    std::string sha = sha1_to_hex(raw.substr(null_pos + 1, 20));

    // create GitTreeLeaf object
    GitTreeLeaf leaf = {mode, path, sha};

    // next offset immediately after the 20-byte SHA
    size_t next = null_pos + 1 + 20;
    return std::make_pair(leaf, next);
}


// tree_parse
std::vector<GitTreeLeaf> tree_parse(const std::string& raw) {
    // start at offset 0
    size_t offset = 0;
    // create vector to store leaves
    std::vector<GitTreeLeaf> leaves;

    // while offset is less than raw size
    while (offset < raw.size()) {
        // parse one leaf and get next offset
        auto [leaf, next] = tree_parse_one(raw, offset);
        // add leaf to vector
        leaves.push_back(leaf);
        // update offset
        offset = next;
    }

    // return the vector of leaves
    return leaves;
}

// tree_leaf_sort_key
std::string tree_leaf_sort_key(const GitTreeLeaf& leaf) {
    // if the mode is 40000, prepend a '/' to the path (to indicate directory)
    if (leaf.mode == "40000") {
        return "/" + leaf.path;
    } else {
        // otherwise, return the path as is
        return leaf.path;
    }
}

// tree_serialize
std::string tree_serialize(std::vector<GitTreeLeaf> leaves) {
    // sort leaves using tree_leaf_sort_key, ascending order
    std::sort(leaves.begin(), leaves.end(), [](const GitTreeLeaf& a, const GitTreeLeaf& b) {
        return tree_leaf_sort_key(a) < tree_leaf_sort_key(b);
    });

    // create string to store result
    std::string result;

    // for each leaf in leaves
    for (const auto& leaf : leaves) {
        // append mode + space + path + null terminator
        result += leaf.mode + " " + leaf.path + '\0';

        // convert sha from hex to raw bytes and append
        for (size_t i = 0; i < leaf.sha.length(); i += 2) {
            std::string byte_string = leaf.sha.substr(i, 2);
            char byte = static_cast<char>(std::stoul(byte_string, nullptr, 16));
            result += byte;
        }
    }

    return result;
}

