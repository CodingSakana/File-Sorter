#include "file_sorter/event_cluster.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace file_sorter {

std::string EventCluster::formatDate(const std::chrono::system_clock::time_point& tp) {
    auto in_time_t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    // 使用本地时间转换为 YYYYMMDD，使用 std::put_time 保证跨编译器兼容性
    ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d");
    return ss.str();
}

GeoLocation EventCluster::findBestLocation(const std::vector<PhotoMetadata>& groupPhotos) {
    if (groupPhotos.empty()) {
        return GeoLocation{};
    }

    int size = groupPhotos.size();
    int mid = size / 2;

    // 1. 检查中位数照片是否带有有效 GPS
    if (groupPhotos[mid].location.isValid) {
        return groupPhotos[mid].location;
    }

    // 2. 双向指针回退寻找：从中间向两端扩散，寻找时间上最接近中心点的坐标
    int left = mid - 1;
    int right = mid + 1;

    while (left >= 0 || right < size) {
        // 先检查右侧（时间稍晚）
        if (right < size && groupPhotos[right].location.isValid) {
            return groupPhotos[right].location;
        }
        // 再检查左侧（时间稍早）
        if (left >= 0 && groupPhotos[left].location.isValid) {
            return groupPhotos[left].location;
        }
        
        left--;
        right++;
    }

    // 整组都没有 GPS 数据
    return GeoLocation{};
}

std::vector<EventGroup> EventCluster::clusterPhotos(std::vector<PhotoMetadata>& photos) {
    std::vector<EventGroup> groups;
    if (photos.empty()) return groups;

    // 1. 按拍摄时间进行全局升序排序
    std::sort(photos.begin(), photos.end(), [](const PhotoMetadata& a, const PhotoMetadata& b) {
        return a.captureTime < b.captureTime;
    });

    // 2. 遍历并分组
    EventGroup currentGroup;
    currentGroup.dateString = formatDate(photos[0].captureTime);
    currentGroup.photos.push_back(photos[0]);

    for (size_t i = 1; i < photos.size(); ++i) {
        const auto& photo = photos[i];
        std::string dateStr = formatDate(photo.captureTime);
        
        // 计算与上一张照片的时间差（秒）
        auto timeDiff = std::chrono::duration_cast<std::chrono::seconds>(
            photo.captureTime - photos[i-1].captureTime).count();

        // 分组触发条件：日期发生改变，或者相邻两张照片拍摄间隔超过 4 小时 (14400 秒)
        // 4小时阈值用于切分同一天内的早晚不同拍摄任务，或处理跨夜星空延时的断档
        if (dateStr != currentGroup.dateString || timeDiff > 14400) {
            // 组装当前组的最佳 GPS 坐标并入库
            currentGroup.medianLocation = findBestLocation(currentGroup.photos);
            groups.push_back(currentGroup);
            
            // 初始化新组
            currentGroup = EventGroup{};
            currentGroup.dateString = dateStr;
        }
        
        currentGroup.photos.push_back(photo);
    }

    // 压入遍历结束时遗留的最后一个组
    if (!currentGroup.photos.empty()) {
        currentGroup.medianLocation = findBestLocation(currentGroup.photos);
        groups.push_back(currentGroup);
    }

    return groups;
}

} // namespace file_sorter