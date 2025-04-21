// ConfigLoader.h
#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <bits/stdc++.h>

class ConfigLoader {
private:
    // Private constructor
    ConfigLoader();
    
    ConfigLoader(const ConfigLoader&) = delete;
    ConfigLoader& operator=(const ConfigLoader&) = delete;
    
    std::unordered_map<std::string, std::string> configData;
    
    std::string configFilePath;
    
    static std::mutex mutex_;
    
    static std::unique_ptr<ConfigLoader> instance;
    
    bool parseConfigFile();
    void trim(std::string& str);

public:
    // Get singleton instance
    static ConfigLoader& getInstance();
    
    bool initialize(const std::string& configFile);
    
    template<typename T>
    T getValue(const std::string& key, const T& defaultValue) const;
    
    bool hasKey(const std::string& key) const;
    
    bool reload();
    
    std::vector<std::string> getAllKeys() const;
};

// Template specializations
template<>
inline std::string ConfigLoader::getValue<std::string>(const std::string& key, const std::string& defaultValue) const {
    auto it = configData.find(key);
    if (it != configData.end()) {
        return it->second;
    }
    return defaultValue;
}

template<>
inline int ConfigLoader::getValue<int>(const std::string& key, const int& defaultValue) const {
    auto it = configData.find(key);
    if (it != configData.end()) {
        try {
            return std::stoi(it->second);
        } catch (...) {
            return defaultValue;
        }
    }
    return defaultValue;
}

template<>
inline double ConfigLoader::getValue<double>(const std::string& key, const double& defaultValue) const {
    auto it = configData.find(key);
    if (it != configData.end()) {
        try {
            return std::stod(it->second);
        } catch (...) {
            return defaultValue;
        }
    }
    return defaultValue;
}

template<>
inline bool ConfigLoader::getValue<bool>(const std::string& key, const bool& defaultValue) const {
    auto it = configData.find(key);
    if (it != configData.end()) {
        std::string value = it->second;
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        if (value == "true" || value == "yes" || value == "1") {
            return true;
        } else if (value == "false" || value == "no" || value == "0") {
            return false;
        }
    }
    return defaultValue;
}

#endif // CONFIG_LOADER_H