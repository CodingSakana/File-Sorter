#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

namespace file_sorter
{

enum class FileFormat { RAW, JPEG, VIDEO, UNKNOWN };

// 地理位置结构体
struct GeoLocation {
    double latitude = 0.0;
    double longitude = 0.0;
    bool isValid = false; // 判断 Exif 中是否成功提取到 GPS
};

// 单张照片的核心元数据
struct PhotoMetadata {
    fs::path originalPath; // 原始文件路径
    FileFormat format;     // 归档分组依据 (RAW/JPEG)

    // 使用 std::chrono 方便后续做时间间隔计算和排序
    std::chrono::system_clock::time_point captureTime;
    bool hasCaptureTime = false; // 极少数照片可能缺失时间元数据

    GeoLocation location; // GPS 坐标
};

// 按时间聚类后的事件组
struct EventGroup {
    std::string dateString;            // 格式化日期，如 "20231024" (YYYYMMDD)
    std::vector<PhotoMetadata> photos; // 该组内的所有照片

    // 以下字段在 API 逆向地理编码后填充
    GeoLocation medianLocation; // 中位数照片的坐标 (或回退后找到的坐标)
    std::string locationName;   // 解析出的地名，如 "Chengdu"
};

} // namespace file_sorter