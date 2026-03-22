#include "file_sorter/scanner.hpp"
#include <algorithm>
#include <cctype>
#include <iostream> // 用于输出可能被吞噬的严重错误

namespace file_sorter {

// determineFormat 保持不变
FileFormat Scanner::determineFormat(const fs::path& filePath) {
    if (!filePath.has_extension()) {
        return FileFormat::UNKNOWN;
    }

    std::string ext = filePath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // 将 PNG/HEIC 等常见非 RAW 图像格式与 JPEG 归为一类进行处理。
    // 注意：这仅影响归档时的子目录（如 "JPEG"），并不保证它们含有与相机照片相同的 EXIF 元数据。
    if (ext == ".jpg" || ext == ".jpeg" || ext == ".heic" || ext == ".png") {
        return FileFormat::JPEG;
    } else if (ext == ".arw" || ext == ".cr2" || ext == ".cr3" || 
               ext == ".nef" || ext == ".dng" || ext == ".raf") {
        return FileFormat::RAW;
    } else if (ext == ".mp4" || ext == ".mov" || ext == ".avi" || 
               ext == ".mkv" || ext == ".flv" || ext == ".wmv" || ext == ".mpg") {
        return FileFormat::VIDEO;
    }

    return FileFormat::UNKNOWN;
}

std::vector<fs::path> Scanner::scanDirectory(const fs::path& rootPath) {
    std::vector<fs::path> imageFiles;

    std::error_code ec;
    if (!fs::exists(rootPath, ec) || !fs::is_directory(rootPath, ec)) {
        return imageFiles; // 目录无效或被删除则直接返回空
    }

    // 设置迭代器选项：遇到没有权限读取的目录时静默跳过
    auto options = fs::directory_options::skip_permission_denied;
    auto it = fs::recursive_directory_iterator(rootPath, options, ec);
    auto end = fs::recursive_directory_iterator();

    while (it != end) {
        if (ec) {
            // 如果在迭代过程中遇到文件系统错误（如文件突然消失），手动步进并清除错误状态
            it.increment(ec);
            continue;
        }

        // 提取文件属性时也使用不抛异常的重载
        if (it->is_regular_file(ec)) {
            FileFormat format = determineFormat(it->path());
            if (format != FileFormat::UNKNOWN) {
                imageFiles.push_back(it->path());
            }
        }
        
        it.increment(ec);
    }

    return imageFiles;
}

} // namespace file_sorter