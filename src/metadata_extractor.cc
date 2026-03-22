#include "file_sorter/metadata_extractor.hpp"
#include <exiv2/exiv2.hpp>
#include <sstream>
#include <iomanip>
#include <vector>
#include <iostream>
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

namespace file_sorter {

// 跨平台 UTC 时间转换，避免 std::mktime 的本地时区污染
inline time_t timegm_cross(struct tm *tm) {
#if defined(_WIN32)
    return _mkgmtime(tm);
#else
    return timegm(tm);
#endif
}

std::chrono::system_clock::time_point MetadataExtractor::parseExifTime(const std::string& timeStr, bool& success) {
    std::tm tm = {};
    std::stringstream ss(timeStr);
    ss >> std::get_time(&tm, "%Y:%m:%d %H:%M:%S");
    
    if (ss.fail()) {
        success = false;
        return {};
    }
    
    success = true;
    tm.tm_isdst = -1;
    // 使用 timegm_cross 确保将 Exif 时间作为 UTC 处理，防止时间戳偏移
    return std::chrono::system_clock::from_time_t(timegm_cross(&tm));
}

double MetadataExtractor::calculateDecimalDegrees(const std::string& rationalStr, const std::string& ref) {
    std::vector<double> parts;
    std::stringstream ss(rationalStr);
    std::string token;
    
    while (std::getline(ss, token, ' ')) {
        size_t slashPos = token.find('/');
        if (slashPos != std::string::npos) {
            double num = std::stod(token.substr(0, slashPos));
            double den = std::stod(token.substr(slashPos + 1));
            parts.push_back(den != 0 ? num / den : 0);
        }
    }

    double degrees = 0.0;
    if (parts.size() >= 1) degrees += parts[0];
    if (parts.size() >= 2) degrees += parts[1] / 60.0;
    if (parts.size() >= 3) degrees += parts[2] / 3600.0;

    if (ref == "S" || ref == "W") {
        degrees = -degrees;
    }

    return degrees;
}

PhotoMetadata MetadataExtractor::extract(const fs::path& filePath, FileFormat format) {
    PhotoMetadata meta;
    meta.originalPath = filePath;
    meta.format = format;
    meta.location.isValid = false;

    // 1. 优先获取文件系统修改时间，作为全局基础保底
    // 这样无论后续 Exiv2 是否抛出异常（如处理不支持的 MPG 视频），都至少有修改时间可用
    try {
        auto ftime = fs::last_write_time(filePath);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        meta.captureTime = sctp;
        meta.hasCaptureTime = true; 
    } catch (const std::exception& e) {
        // 极少数情况下读取文件系统时间失败（如权限问题）
        meta.hasCaptureTime = false;
        std::cerr << "[Warning] Failed to read filesystem time for " << filePath << std::endl;
    }

    try {
        // 2. 尝试使用 Exiv2 读取元数据
        auto image = Exiv2::ImageFactory::open(filePath.string());
        image->readMetadata();
        Exiv2::ExifData &exifData = image->exifData();

        bool exifParseSuccess = false;
        std::chrono::system_clock::time_point exifTime;

        auto itOrig = exifData.findKey(Exiv2::ExifKey("Exif.Photo.DateTimeOriginal"));
        auto itTime = exifData.findKey(Exiv2::ExifKey("Exif.Image.DateTime"));
        
        if (itOrig != exifData.end()) {
            exifTime = parseExifTime(itOrig->toString(), exifParseSuccess);
        } else if (itTime != exifData.end()) {
            exifTime = parseExifTime(itTime->toString(), exifParseSuccess);
        }

        // 3. 校验 Exif 时间有效性，若有效且不是1970年，则覆盖基础保底时间
        if (exifParseSuccess) {
            std::time_t capTimeT = std::chrono::system_clock::to_time_t(exifTime);
            std::tm* tmPtr = std::gmtime(&capTimeT);
            
            // 只有当解析出的 Exif 年份不是 1970 时，才覆盖 meta.captureTime
            if (tmPtr && tmPtr->tm_year != 70) {
                meta.captureTime = exifTime;
            } else {
                std::cerr << "[Info] Exif time is 1970, falling back to FS time for " << filePath << std::endl;
            }
        }

        // 4. 提取 GPS (逻辑保持不变)
        auto itLat = exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLatitude"));
        auto itLatRef = exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLatitudeRef"));
        auto itLon = exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLongitude"));
        auto itLonRef = exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLongitudeRef"));

        if (itLat != exifData.end() && itLatRef != exifData.end() &&
            itLon != exifData.end() && itLonRef != exifData.end()) {
            
            meta.location.latitude = calculateDecimalDegrees(itLat->toString(), itLatRef->toString());
            meta.location.longitude = calculateDecimalDegrees(itLon->toString(), itLonRef->toString());
            meta.location.isValid = true;
        }

    } catch (const Exiv2::Error& e) {
        // 对于 MPG 文件，代码会跳到这里。
        // 由于 meta 已经在 try 块外部被赋予了文件系统时间，它将被正确归类到 2009 年。
        std::cerr << "[Warning] Exiv2 skipped (unsupported or no metadata) " 
                  << filePath << " : " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[Error] Unexpected error processing " 
                  << filePath << " : " << e.what() << std::endl;
    }

    return meta;
}

} // namespace file_sorter