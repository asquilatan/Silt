#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include "Repository.hpp"

class IndexEntry {
public:
    // File metadata
    std::string path;
    
    // Staging area metadata (from .git/index)
    int ctime_sec;
    int ctime_nsec;
    int mtime_sec;
    int mtime_nsec;
    int dev;
    int ino;
    int mode;
    int uid;
    int gid;
    int file_size;
    std::string sha;
    int flags;
    
    IndexEntry() : ctime_sec(0), ctime_nsec(0), mtime_sec(0), mtime_nsec(0),
                   dev(0), ino(0), mode(0), uid(0), gid(0), file_size(0), flags(0) {}
    
    IndexEntry(const std::string& p) : path(p), ctime_sec(0), ctime_nsec(0), 
                                        mtime_sec(0), mtime_nsec(0), dev(0), ino(0), 
                                        mode(0), uid(0), gid(0), file_size(0), flags(0) {}
};

class Index {
public:
    Index() = default;
    explicit Index(const Repository& repo);
    
    // Load index from file
    bool read(const Repository& repo);
    
    // Write index to file
    bool write(const Repository& repo) const;
    
    // Add entry to index
    void add_entry(const IndexEntry& entry);
    
    // Remove entry from index
    bool remove_entry(const std::string& path);
    
    // Get entry by path
    std::optional<IndexEntry> get_entry(const std::string& path) const;
    
    // Get all entries
    const std::vector<IndexEntry>& get_entries() const;
    
    // Clear all entries
    void clear();
    
    // Check if index has changes
    bool has_changes() const;
    
    // Get index file path
    static std::filesystem::path get_index_path(const Repository& repo);
    
private:
    std::vector<IndexEntry> entries;
    
    // Helper functions for reading/writing
    std::string serialize_entry(const IndexEntry& entry) const;
    std::optional<std::pair<IndexEntry, size_t>> deserialize_entry(const std::string& data, size_t offset) const;
};
