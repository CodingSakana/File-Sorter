#pragma once

#include "file_sorter/types.hpp"
#include <vector>

namespace file_sorter {

class EventCluster {
public:
    // 将提取好元数据的照片列表进行时间排序和聚类
    static std::vector<EventGroup> clusterPhotos(std::vector<PhotoMetadata>& photos);

private:
    // 将 time_point 格式化为 YYYYMMDD 字符串
    static std::string formatDate(const std::chrono::system_clock::time_point& tp);
    
    // 在一个事件组内寻找最优的 GPS 坐标（中位数及双向回退逻辑）
    static GeoLocation findBestLocation(const std::vector<PhotoMetadata>& groupPhotos);
};

} // namespace file_sorter