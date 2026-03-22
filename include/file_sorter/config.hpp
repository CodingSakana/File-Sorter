#pragma once

#include <string>

namespace file_sorter {

struct Config {
    std::string source_dir = "./test_photos";
    std::string target_dir = "./sorted_photos";
    std::string log_path = "logs/file_sorter.log";
    std::string geo_cache_path = "data/geo_cache.json";
    unsigned int thread_count = 0; // 0 means auto-detect
    bool copy_only = true;

    static Config& getInstance() {
        static Config instance;
        return instance;
    }

    void load(const std::string& configFilePath = "config.yaml");

private:
    Config() = default;
};

} // namespace file_sorter