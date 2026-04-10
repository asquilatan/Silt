#include "Index.hpp"
#include "Utils.hpp"
#include <fstream>
#include <algorithm>
#include <cstdio>
#include <openssl/sha.h>

namespace {
void append_u32(std::string& out, int value) {
    const unsigned int v = static_cast<unsigned int>(value);
    out += static_cast<char>((v >> 24) & 0xFF);
    out += static_cast<char>((v >> 16) & 0xFF);
    out += static_cast<char>((v >> 8) & 0xFF);
    out += static_cast<char>(v & 0xFF);
}

void append_u16(std::string& out, int value) {
    const unsigned int v = static_cast<unsigned int>(value) & 0xFFFF;
    out += static_cast<char>((v >> 8) & 0xFF);
    out += static_cast<char>(v & 0xFF);
}

int read_u32(const std::string& data, size_t offset) {
    return (static_cast<unsigned char>(data[offset]) << 24) |
           (static_cast<unsigned char>(data[offset + 1]) << 16) |
           (static_cast<unsigned char>(data[offset + 2]) << 8) |
           (static_cast<unsigned char>(data[offset + 3]));
}

int read_u16(const std::string& data, size_t offset) {
    return (static_cast<unsigned char>(data[offset]) << 8) |
           (static_cast<unsigned char>(data[offset + 1]));
}
}

Index::Index(const Repository& repo) {
    read(repo);
}

std::filesystem::path Index::get_index_path(const Repository& repo) {
    return repo.gitdir / "index";
}

bool Index::read(const Repository& repo) {
    entries.clear();
    
    std::filesystem::path index_path = get_index_path(repo);
    
    // If index file doesn't exist, that's OK - it just means empty index
    if (!std::filesystem::exists(index_path)) {
        return true;
    }
    
    // Read the entire file
    std::ifstream file(index_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Read all content
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    if (content.size() < 12) {
        // Index file too small to be valid
        return false;
    }
    
    // Check header: "DIRC" (Git index signature)
    if (content.substr(0, 4) != "DIRC") {
        return false;
    }
    
    // Parse version (bytes 4-7, big-endian)
    int version = read_u32(content, 4);
    
    if (version != 2) {
        return false; // Only support version 2
    }
    
    // Parse entry count (bytes 8-11, big-endian)
    int num_entries = read_u32(content, 8);
    
    // Parse entries
    size_t offset = 12;
    for (int i = 0; i < num_entries; i++) {
        auto result = deserialize_entry(content, offset);
        if (!result.has_value()) {
            return false;
        }
        entries.push_back(result->first);
        offset = result->second;
    }
    
    return true;
}

bool Index::write(const Repository& repo) const {
    std::filesystem::path index_path = get_index_path(repo);
    
    // Ensure directory exists
    std::filesystem::create_directories(index_path.parent_path());
    
    // Build the index file content
    std::string content;
    
    // Header: "DIRC"
    content += "DIRC";
    
    // Version: 2
    append_u32(content, 2);
    
    // Number of entries
    append_u32(content, static_cast<int>(entries.size()));
    
    // Serialize all entries
    for (const auto& entry : entries) {
        content += serialize_entry(entry);
    }
    
    // Calculate and append SHA-1 hash of the content
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(content.c_str()), content.size(), hash);
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        content += static_cast<char>(hash[i]);
    }
    
    // Write to file
    std::ofstream file(index_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    file.write(content.c_str(), content.size());
    file.close();
    
    return true;
}

std::string Index::serialize_entry(const IndexEntry& entry) const {
    std::string result;
    
    // 10 fixed 32-bit metadata fields
    append_u32(result, entry.ctime_sec);
    append_u32(result, entry.ctime_nsec);
    append_u32(result, entry.mtime_sec);
    append_u32(result, entry.mtime_nsec);
    append_u32(result, entry.dev);
    append_u32(result, entry.ino);
    append_u32(result, entry.mode);
    append_u32(result, entry.uid);
    append_u32(result, entry.gid);
    append_u32(result, entry.file_size);
    
    // SHA-1 hash (40 characters hex string converted to 20 bytes)
    if (entry.sha.size() == 40) {
        for (size_t i = 0; i < 40; i += 2) {
            unsigned int byte = 0;
            for (int j = 0; j < 2; j++) {
                byte = byte * 16;
                char c = entry.sha[i + j];
                if (c >= '0' && c <= '9') {
                    byte += c - '0';
                } else if (c >= 'a' && c <= 'f') {
                    byte += c - 'a' + 10;
                } else if (c >= 'A' && c <= 'F') {
                    byte += c - 'A' + 10;
                }
            }
            result += static_cast<char>(byte);
        }
    }
    
    // flags (2 bytes, big-endian)
    append_u16(result, entry.flags);
    
    // path (variable length, null-terminated)
    result += entry.path;
    result += '\0';
    
    // Padding: pad to 8-byte boundary
    while (result.size() % 8 != 0) {
        result += '\0';
    }
    
    return result;
}

std::optional<std::pair<IndexEntry, size_t>> Index::deserialize_entry(const std::string& data, size_t offset) const {
    if (offset + 62 > data.size()) {
        return std::nullopt;
    }
    
    IndexEntry entry;
    
    entry.ctime_sec = read_u32(data, offset + 0);
    entry.ctime_nsec = read_u32(data, offset + 4);
    entry.mtime_sec = read_u32(data, offset + 8);
    entry.mtime_nsec = read_u32(data, offset + 12);
    entry.dev = read_u32(data, offset + 16);
    entry.ino = read_u32(data, offset + 20);
    entry.mode = read_u32(data, offset + 24);
    entry.uid = read_u32(data, offset + 28);
    entry.gid = read_u32(data, offset + 32);
    entry.file_size = read_u32(data, offset + 36);
    
    // SHA-1 hash (20 bytes -> 40 hex chars)
    for (int i = 0; i < 20; i++) {
        unsigned char byte = static_cast<unsigned char>(data[offset + 40 + i]);
        char hex[3];
        snprintf(hex, sizeof(hex), "%02x", byte);
        entry.sha += hex;
    }
    
    // flags
    entry.flags = read_u16(data, offset + 60);
    
    // path (variable length, null-terminated)
    size_t path_start = offset + 62;
    size_t path_end = data.find('\0', path_start);
    if (path_end == std::string::npos) {
        return std::nullopt;
    }
    
    entry.path = data.substr(path_start, path_end - path_start);
    
    // Skip padding to 8-byte boundary (alignment is relative to entry start).
    size_t entry_len = (path_end - offset) + 1; // include null terminator
    size_t padded_len = (entry_len + 7) & ~static_cast<size_t>(7);
    size_t entry_end = offset + padded_len;
    
    return std::make_pair(entry, entry_end);
}

void Index::add_entry(const IndexEntry& entry) {
    // Remove existing entry with same path if it exists
    remove_entry(entry.path);
    
    // Add new entry and sort
    entries.push_back(entry);
    std::sort(entries.begin(), entries.end(), 
              [](const IndexEntry& a, const IndexEntry& b) { return a.path < b.path; });
}

bool Index::remove_entry(const std::string& path) {
    auto it = std::find_if(entries.begin(), entries.end(),
                          [&path](const IndexEntry& e) { return e.path == path; });
    if (it != entries.end()) {
        entries.erase(it);
        return true;
    }
    return false;
}

std::optional<IndexEntry> Index::get_entry(const std::string& path) const {
    auto it = std::find_if(entries.begin(), entries.end(),
                          [&path](const IndexEntry& e) { return e.path == path; });
    if (it != entries.end()) {
        return *it;
    }
    return std::nullopt;
}

const std::vector<IndexEntry>& Index::get_entries() const {
    return entries;
}

void Index::clear() {
    entries.clear();
}

bool Index::has_changes() const {
    return !entries.empty();
}
