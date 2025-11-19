#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>

class ConfigParser {
private:
    std::map<std::string, std::map<std::string, std::string>> sections;
    
public:
    // Constructor
    ConfigParser() {}
    
    // Parse a config file
    bool read(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        
        std::string line;
        std::string currentSection = "DEFAULT";
        sections[currentSection] = std::map<std::string, std::string>();
        
        while (std::getline(file, line)) {
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#' || line[0] == ';') {
                continue;
            }
            
            // Check for section header [section]
            if (line[0] == '[' && line[line.length() - 1] == ']') {
                currentSection = line.substr(1, line.length() - 2);
                // Convert to lowercase for case-insensitive matching
                std::transform(currentSection.begin(), currentSection.end(), currentSection.begin(), ::tolower);
                sections[currentSection] = std::map<std::string, std::string>();
                continue;
            }
            
            // Parse key=value pairs
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                
                // Trim key and value
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                
                sections[currentSection][key] = value;
            }
        }
        
        file.close();
        return true;
    }
    
    // Get a value from a specific section
    std::string get(const std::string& section, const std::string& key, const std::string& defaultValue = "") const {
        std::string lowerSection = section;
        std::transform(lowerSection.begin(), lowerSection.end(), lowerSection.begin(), ::tolower);
        
        auto sectionIt = sections.find(lowerSection);
        if (sectionIt != sections.end()) {
            auto keyIt = sectionIt->second.find(key);
            if (keyIt != sectionIt->second.end()) {
                return keyIt->second;
            }
        }
        return defaultValue;
    }
    
    // Check if a section exists
    bool hasSection(const std::string& section) const {
        std::string lowerSection = section;
        std::transform(lowerSection.begin(), lowerSection.end(), lowerSection.begin(), ::tolower);
        return sections.find(lowerSection) != sections.end();
    }
    
    // Check if a key exists in a section
    bool hasOption(const std::string& section, const std::string& key) const {
        std::string lowerSection = section;
        std::transform(lowerSection.begin(), lowerSection.end(), lowerSection.begin(), ::tolower);
        
        auto sectionIt = sections.find(lowerSection);
        if (sectionIt != sections.end()) {
            return sectionIt->second.find(key) != sectionIt->second.end();
        }
        return false;
    }
    
    // Add or update a value
    void set(const std::string& section, const std::string& key, const std::string& value) {
        std::string lowerSection = section;
        std::transform(lowerSection.begin(), lowerSection.end(), lowerSection.begin(), ::tolower);
        sections[lowerSection][key] = value;
    }

    // Convert the config to a string
    std::string toString() const {
        std::stringstream ss;
        for (const auto& section_pair : sections) {
            // Don't write the DEFAULT section if it's empty or was just a placeholder
            if (section_pair.first == "DEFAULT" && section_pair.second.empty()) {
                continue;
            }
            // Create the section
            ss << "[" << section_pair.first << "]\n";
            // For every key val pair in the section
            for (const auto& key_value_pair : section_pair.second) {
                // Have this be a part of the string
                ss << "\t" << key_value_pair.first << " = " << key_value_pair.second << "\n";
            }
        }
        return ss.str();
    }
};

#endif // UTILS_HPP