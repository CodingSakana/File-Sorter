#include "file_sorter/event_cluster.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <map>

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
    // This ensures that when we iterate and group, photos within each group are already time-ordered.
    std::sort(photos.begin(), photos.end(), [](const PhotoMetadata& a, const PhotoMetadata& b) {
        return a.captureTime < b.captureTime;
    });

    // 2. 使用 map 按日期严格分组，确保每天只有一个分组
    std::map<std::string, std::vector<PhotoMetadata>> photosByDate;
    for (const auto& photo : photos) {
        photosByDate[formatDate(photo.captureTime)].push_back(photo);
    }

    // 3. 从 map 创建 EventGroup 列表
    // 预分配空间以提高效率
    groups.reserve(photosByDate.size());
    for (auto const& [dateStr, datePhotos] : photosByDate) {
        EventGroup group;
        group.dateString = dateStr;
        // 由于我们是按已排序的 `photos` 列表填充 map 的，
        // `datePhotos` 内部的照片也保持了时间顺序。
        group.photos = datePhotos;
        group.medianLocation = findBestLocation(group.photos);
        groups.push_back(std::move(group));
    }

    return groups;
}

} // namespace file_sorter