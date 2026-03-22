#pragma once

#include "file_sorter/types.hpp"
#include <string>
#include <unordered_map>

namespace file_sorter {

class GeoResolver {
public:
    // 初始化并加载本地缓存
    static void initCache(const std::string& cachePath = "data/geo_cache.json");
    
    // 将内存缓存持久化到磁盘
    static void saveCache();
    
    // 输入坐标，返回解析后的地名（如 "Chengdu"）。内部包含缓存命中和 API 回退逻辑
    static std::string resolve(const GeoLocation& loc);

private:
    static std::unordered_map<std::string, std::string> cache_;
    static std::string cacheFilePath_;

    // 生成缓存的 Key。对经纬度保留2位小数（约 1.1km 精度），提高同城拍摄的缓存命中率
    static std::string makeCacheKey(const GeoLocation& loc);
    
    // 实际发起 HTTP 请求调用 OpenStreetMap API
    static std::string fetchFromAPI(const GeoLocation& loc);
};

} // namespace file_sorter