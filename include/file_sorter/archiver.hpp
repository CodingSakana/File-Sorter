#pragma once

#include "file_sorter/types.hpp"
#include <vector>

class ThreadPool; // Forward declaration

namespace file_sorter {

class Archiver {
public:
    // 执行归档任务
    // copyOnly: 若为 true 则复制文件，false 则移动文件（剪切）
    static void execute(const std::vector<EventGroup>& groups, 
                       const fs::path& targetRoot, 
                       ThreadPool& pool,
                       bool copyOnly = true);

private:
    // 辅助函数：根据格式返回子目录名称
    static std::string getFormatDirName(FileFormat format);
};

} // namespace file_sorter