#pragma once

#include "file_sorter/types.hpp"
#include <string>

namespace file_sorter {

class MetadataExtractor {
public:
    // 解析单张照片的元数据
    static PhotoMetadata extract(const fs::path& filePath, FileFormat format);

private:
    // 将 EXIF 标准时间字符串 (YYYY:MM:DD HH:MM:SS) 转换为 chrono time_point
    static std::chrono::system_clock::time_point parseExifTime(const std::string& timeStr, bool& success);
    
    // 辅助函数：从 Exiv2 的 Rational 格式中提取并计算十进制经纬度
    static double calculateDecimalDegrees(const std::string& rationalStr, const std::string& ref);
};

} // namespace file_sorter