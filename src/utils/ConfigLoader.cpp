// ConfigLoader.cpp
#include <algorithm>
#include <cctype>
#include <vector>
#include "utils/ConfigLoader.hpp"

std::mutex ConfigLoader::mutex_;
std::unique_ptr<ConfigLoader> ConfigLoader::instance = nullptr;

ConfigLoader::ConfigLoader() : configFilePath("") {}

ConfigLoader& ConfigLoader::getInstance() {
    if (instance == nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (instance == nullptr) {
            instance.reset(new ConfigLoader());
        }
    }
    return *instance;
}

bool ConfigLoader::initialize(const std::string& configFile) {
    std::lock_guard<std::mutex> lock(mutex_);
    configFilePath = configFile;
    return parseConfigFile();
}

bool ConfigLoader::parseConfigFile() {
    std::ifstream file(configFilePath);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << configFilePath << std::endl;
        return false;
    }

    configData.clear();
    std::string line;
    
    while (std::getline(file, line)) {
        trim(line);
        
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        size_t separatorPos = line.find('=');
        if (separatorPos != std::string::npos) {
            std::string key = line.substr(0, separatorPos);
            std::string value = line.substr(separatorPos + 1);
            trim(key);
            trim(value);
            configData[key] = value;
        }
    }
    
    file.close();
    return true;
}

void ConfigLoader::trim(std::string& str) {
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    
    str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), str.end());
}

bool ConfigLoader::hasKey(const std::string& key) const {
    return configData.find(key) != configData.end();
}

bool ConfigLoader::reload() {
    std::lock_guard<std::mutex> lock(mutex_);
    return parseConfigFile();
}

std::vector<std::string> ConfigLoader::getAllKeys() const {
    std::vector<std::string> keys;
    keys.reserve(configData.size());
    
    for (const auto& pair : configData) {
        keys.push_back(pair.first);
    }
    
    return keys;
}