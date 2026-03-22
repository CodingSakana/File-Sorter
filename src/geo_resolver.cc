#include "file_sorter/geo_resolver.hpp"
#include "file_sorter/logger.hpp"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>

using json = nlohmann::json;

namespace file_sorter {

std::unordered_map<std::string, std::string> GeoResolver::cache_;
std::string GeoResolver::cacheFilePath_;

void GeoResolver::initCache(const std::string& cachePath) {
    cacheFilePath_ = cachePath;
    std::ifstream file(cacheFilePath_);
    if (file.is_open()) {
        try {
            json j;
            file >> j;
            cache_ = j.get<std::unordered_map<std::string, std::string>>();
        } catch (const json::exception& e) {
            // 缓存文件损坏或为空，静默处理，按空缓存启动
        }
    }
}

void GeoResolver::saveCache() {
    if (cache_.empty()) return;

    // 确保数据目录存在
    fs::path path(cacheFilePath_);
    if (path.has_parent_path() && !fs::exists(path.parent_path())) {
        fs::create_directories(path.parent_path());
    }

    std::ofstream file(cacheFilePath_);
    if (file.is_open()) {
        json j(cache_);
        file << j.dump(4); // 格式化输出，缩进 4 个空格
    }
}

std::string GeoResolver::makeCacheKey(const GeoLocation& loc) {
    // 经纬度保留 2 位小数，相当于将相距 1 公里内的照片归为同一地点，大幅减少 API 请求
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << loc.latitude << "," << loc.longitude;
    return ss.str();
}

std::string GeoResolver::fetchFromAPI(const GeoLocation& loc) {
    std::string url = "https://nominatim.openstreetmap.org/reverse";
    auto response = cpr::Get(
        cpr::Url{url},
        cpr::Parameters{
            {"format", "json"},
            {"lat", std::to_string(loc.latitude)},
            {"lon", std::to_string(loc.longitude)},
            {"zoom", "10"},
            {"accept-language", "zh-CN"}
        },
        cpr::Header{{"User-Agent", "FileSorter/1.0"}}
    );

    if (response.status_code == 200) {
        #ifndef NDEBUG
        FS_TRACE("Raw API Response: {}", response.text);
        #endif
        try {
            json j = json::parse(response.text);
            auto addr = j["address"];
            
            // 1. 先处理直辖市：如果是天津、北京等，state 就是我们要的城市名
            std::string state = addr.value("state", "");
            if (state == "天津市" || state == "北京市" || 
                state == "上海市" || state == "重庆市") {
                // 去除末尾的"市"字
                if (state.length() >= 3) {
                    return state.substr(0, state.length() - 3);
                }
                return state;
            }

            // 2. 如果 address 里有明确的 city 且不含 "区/县"
            if (addr.contains("city")) {
                std::string city = addr["city"];
                if (city.find("区") == std::string::npos && 
                    city.find("县") == std::string::npos) {
                    // 去除末尾的"市"字
                    if (city.length() >= 3 && city.substr(city.length() - 3) == "市") {
                        return city.substr(0, city.length() - 3);
                    }
                    return city;
                }
            }

            // 3. 终极兜底方案：从 display_name 逆向解析
            // display_name 格式通常是: "..., 城市, 省份, [邮编], 国家"
            if (j.contains("display_name")) {
                std::string display = j["display_name"];
                std::vector<std::string> parts;
                std::stringstream ss(display);
                std::string item;
                while (std::getline(ss, item, ',')) {
                    // 去除首尾空格
                    item.erase(0, item.find_first_not_of(" "));
                    item.erase(item.find_last_not_of(" ") + 1);
                    parts.push_back(item);
                }

                // 寻找省份(state)的位置，它前面的通常就是城市
                for (size_t i = 1; i < parts.size(); ++i) {
                    if (parts[i] == state && i > 0) {
                        // 额外检查：防止拿到 "蜀山区"
                        if (parts[i-1].find("区") == std::string::npos &&
                            parts[i-1].find("县") == std::string::npos) {
                            std::string res = parts[i-1];
                            if (res.length() >= 3 && res.substr(res.length() - 3) == "市") {
                                return res.substr(0, res.length() - 3);
                            }
                            return res;
                        }
                        // 如果 i-1 是区，再往前找一个可能就是市了
                        if (i > 1) {
                            std::string res = parts[i-2];
                            if (res.length() >= 3 && res.substr(res.length() - 3) == "市") {
                                return res.substr(0, res.length() - 3);
                            }
                            return res;
                        }
                    }
                }
            }
        } catch (...) {}
    }
    return "Unknown_Location";
}

std::string GeoResolver::resolve(const GeoLocation& loc) {
    if (!loc.isValid) {
        return "Unknown_Location";
    }

    std::string key = makeCacheKey(loc);
    if (cache_.find(key) != cache_.end()) {
        return cache_[key];
    }

    // 缓存未命中，调用 API。必须遵守 1 QPS 的限制
    FS_INFO("Resolving new location from API ({}, {})...", loc.latitude, loc.longitude);
    std::string locationName = fetchFromAPI(loc);
    
    // 只有成功解析到地点才加入缓存
    if (locationName != "Unknown_Location" && !locationName.empty()) {
        cache_[key] = locationName;
    }
    
    // 强制休眠 1 秒，防止触发反爬虫封禁
    std::this_thread::sleep_for(std::chrono::seconds(1));

    return locationName;
}

} // namespace file_sorter