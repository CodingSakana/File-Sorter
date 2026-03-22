#include "file_sorter/config.hpp"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace file_sorter {

void Config::load(const std::string& configFilePath) {
    if (!fs::exists(configFilePath)) {
        // Use defaults if config file doesn't exist
        return;
    }

    try {
        YAML::Node config = YAML::LoadFile(configFilePath);

        if (config["source_dir"]) {
            source_dir = config["source_dir"].as<std::string>();
        }
        if (config["target_dir"]) {
            target_dir = config["target_dir"].as<std::string>();
        }
        if (config["log_path"]) {
            log_path = config["log_path"].as<std::string>();
        }
        if (config["geo_cache_path"]) {
            geo_cache_path = config["geo_cache_path"].as<std::string>();
        }
        if (config["thread_count"]) {
            thread_count = config["thread_count"].as<unsigned int>();
        }
        if (config["copy_only"]) {
            copy_only = config["copy_only"].as<bool>();
        }
    } catch (const YAML::Exception& e) {
        std::cerr << "[Config] Error parsing config.yaml: " << e.what() << std::endl;
        std::cerr << "[Config] Falling back to default settings." << std::endl;
    }
}

} // namespace file_sorter