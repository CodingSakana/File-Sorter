#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include "types.hpp"

namespace fs = std::filesystem;

namespace file_sorter {

class Scanner {
public:
    // 递归扫描目录，返回所有合规的影像文件路径
    static std::vector<fs::path> scanDirectory(const fs::path& rootPath);

    // 根据文件后缀名判断格式 (RAW / JPEG)
    static FileFormat determineFormat(const fs::path& filePath);
};

} // namespace file_sorter