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
#include <iostream>
#include <regex>
#include "Objects.hpp"  // Include the header file to get KVLM types

// Implement the GitBlob constructor that takes a string
GitBlob::GitBlob(const std::string& data) {
    deserialize(data);
}

std::string object_write(std::unique_ptr<GitObject> obj, Repository* repo);

// Implement the GitBlob constructor that takes a string
// For example: 
// 
std::optional<std::unique_ptr<GitObject>> object_read(Repository* repo, char* sha) {
    // first 2 digits of sha
    char* dirname = new char[3];
    strncpy(dirname, sha, 2);
    dirname[2] = '\0';

    // next few digits of sha
    char* filename = new char[strlen(sha) - 1];
    strncpy(filename, sha + 2, strlen(sha) - 2);
    filename[strlen(sha) - 2] = '\0';

    std::filesystem::path path = repo_file(*repo, "objects", dirname, filename, nullptr);

    // if path is not a file
    if (!std::filesystem::is_regular_file(path)) {
        delete[] dirname;
        delete[] filename;
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
        std::string error_msg = "Zlib inflation failed";
        if (zs.msg) {
            error_msg += ": " + std::string(zs.msg);
        } else {
            error_msg += " with code " + std::to_string(ret);
        }
        throw std::runtime_error(error_msg);
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
    SHA1(reinterpret_cast<const unsigned char*>(result.data()), result.length(), hash);

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

        char* filename = new char[strlen(sha_ptr) - 1];
        strncpy(filename, sha_ptr + 2, strlen(sha_ptr) - 2);
        filename[strlen(sha_ptr) - 2] = '\0';

        std::filesystem::path path = repo_file(*repo, "objects", dirname, filename, nullptr);

        if (!std::filesystem::exists(path)) {
            // Create directory if it doesn't exist
            std::filesystem::create_directories(path.parent_path());

            // Compress data
            uLongf compressed_size = compressBound(result.length());
            std::vector<char> compressed_data(compressed_size);
            if (compress(reinterpret_cast<Bytef*>(compressed_data.data()), &compressed_size, reinterpret_cast<const Bytef*>(result.data()), result.length()) != Z_OK) {
                throw std::runtime_error("Failed to compress object data.");
            }
            compressed_data.resize(compressed_size);

            // Write to file
            std::ofstream file(path, std::ios::binary);
            file.write(compressed_data.data(), compressed_data.size());
            file.close();
        }
        delete[] dirname;
        delete[] filename;
    }

    return sha;
}


/**
* Create an array list of candidates.
* Candidates will look like this after: 
* [a94a8fe2b1cd9..., a94a8f56cc6ae..., a94a8f439c9fed3a...]
*/
std::vector<std::string> object_resolve(Repository* repo, std::string name) {
    std::vector<std::string> candidates;

    // create a regex to match hash
    std::regex hashRE("^[0-9A-Fa-f]{4,40}$");

    // if it's empty, return empty vector
    if (name.empty()) {
        return {};
    }

    // if it's HEAD, resolve it to whatever branch HEAD points to, returns a sha
    if (name == "HEAD") {
        auto head = ref_resolve(*repo, "HEAD");
        // if head contains a value, return it in a vector
        if (head) {
            return { *head };
        }
        // otherwise, return an empty vector
        return {};
    }

    // if it's a match, add it to the candidates
    if (std::regex_match(name, hashRE)) {
        // convert the name to lowercase
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        // takes the first 2 characters of the name
        std::string prefix = name.substr(0, 2);
        // creates a path to the directory containing the object, (e.g. .git/objects/a9)
        std::filesystem::path path = repo_dir(*repo, false, "objects", prefix.c_str(), nullptr);
        
        // if the path exists, iterate over the directory and add the filename to the candidates
        if (std::filesystem::exists(path)) {
            // take everything after the first 2 characters
            std::string rem = name.substr(2);
            // for each entry int he directory
            for (const auto& entry : std::filesystem::directory_iterator(path)) {
                // take the filename of the entry
                std::string filename = entry.path().filename().string();
                // if the filename starts with the remaining characters of the name, add it to the candidates
                if (filename.find(rem) == 0) {
                    // add the prefix and filename to the candidates
                    candidates.push_back(prefix + filename);
                }
            }
        }
    }

    // if it's not a hash, it's a reference, resolve it to a sha
    auto as_tag = ref_resolve(*repo, "refs/tags/" + name);
    if (as_tag) {
        candidates.push_back(*as_tag);
    }

    // if it's not a tag, it's a branch, resolve it to a sha
    auto as_branch = ref_resolve(*repo, "refs/heads/" + name);
    if (as_branch) {
        candidates.push_back(*as_branch);
    }
    // if it's not a branch, it's a remote branch, resolve it to a sha
    auto as_remote_branch = ref_resolve(*repo, "refs/remotes/" + name);
    if (as_remote_branch) {
        candidates.push_back(*as_remote_branch);
    }

    // return the vector of candidates
    return candidates;
}


/**
 * Find an object with a given name in the repository.
 * For example:
 * 
 */
std::string object_find(Repository* repo, std::string name, std::string fmt, bool follow) {
    // create a vector of candidates
    auto sha_list = object_resolve(repo, name);

    // if there are no candidates, throw an error
    // {}
    if (sha_list.empty()) {
        throw std::runtime_error("No such reference " + name + ".");
    }

    // if the size is greater than 1, throw an error including all candidates
    // {a94a8fe2b1cd9..., a94a8f56cc6ae...}
    if (sha_list.size() > 1) {
        std::string candidates_str;
        for (const auto& s : sha_list) {
            candidates_str += "\n - " + s;
        }
        throw std::runtime_error("Ambiguous reference " + name + ": Candidates are:" + candidates_str + ".");
    }

    // let the sha be the first item (because there's only ONE)
    std::string sha = sha_list[0];

    // if no format is specified, return the sha
    // this would be used when just resolving a reference without caring about type
    // if fmt is empty, [a94a8fe2b1cd9...] -> return a94a8fe2b1cd9...
    if (fmt.empty()) {
        return sha;
    }

    // if the format is specified, read the object and check the type
    while (true) {
        // read the object
        auto obj_opt = object_read(repo, const_cast<char*>(sha.c_str()));
        // if the object is not found, return an empty string
        if (!obj_opt) {
             return ""; 
        }

        // the object is found, get the object
        auto& obj = *obj_opt;

        // get the object format
        std::string obj_fmt = obj->get_fmt();

        // if the object format matches the requested format, return the sha
        if (obj_fmt == fmt) {
            return sha;
        }

        // if we're not following, return an empty string
        if (!follow) {
            return "";
        }

        // Helper lambda to extract a string field from KVLM
        auto get_kvlm_field = [](const KVLM& kvlm, const std::string& key) -> std::optional<std::string> {
            // get the iterator for the key
            auto it = kvlm.find(key);
            // if the iterator is valid and the value is a string, return the string
            if (it != kvlm.end() && std::holds_alternative<std::string>(it->second)) {
                return std::get<std::string>(it->second);
            }
            // otherwise, return nullopt
            return std::nullopt;
        };

        std::optional<std::string> next_sha;
        
        // if the object is a tag, get the object field
        if (obj_fmt == "tag") {
            if (auto tag = dynamic_cast<GitTag*>(obj.get())) {
                next_sha = get_kvlm_field(tag->get_kvlm(), "object");
            }
        // if the object is a commit and the requested format is tree, get the tree field
        } else if (obj_fmt == "commit" && fmt == "tree") {
            if (auto commit = dynamic_cast<GitCommit*>(obj.get())) {
                next_sha = get_kvlm_field(commit->get_kvlm(), "tree");
            }
        }

        // if we have a next sha, set sha to the next sha
        if (next_sha) {
            sha = *next_sha;
        // otherwise, return an empty string
        } else {
            return "";
        }
    }
    // return an empty string
    return "";
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

std::pair<GitTreeLeaf, size_t> tree_parse_one(const std::string& raw, size_t start) {
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
    // if it starts with 10, return leaf path
    if (leaf.mode.substr(0, 2) == "10") {
        return leaf.path;
    }
    // otherwise, return leaf path + '/'
    return leaf.path + "/";
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

// GitTree implementations
std::string GitTree::serialize() {
    return tree_serialize(leaves);
}

void GitTree::deserialize(const std::string& data) {
    leaves = tree_parse(data);
}

const std::vector<GitTreeLeaf>& GitTree::get_leaves() const {
    return leaves;
}

void GitTree::set_leaves(const std::vector<GitTreeLeaf>& new_leaves) {
    leaves = new_leaves;
}

void GitTree::add_leaf(const GitTreeLeaf& leaf) {
    leaves.push_back(leaf);
}

bool GitTree::empty() const {
    return leaves.empty();
}

void GitTree::initialize() {
    leaves.clear();
}

