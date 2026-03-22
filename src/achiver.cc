#include "file_sorter/archiver.hpp"
#include "file_sorter/thread_pool.hpp"
#include "file_sorter/logger.hpp"
#include <format>
#include <unordered_set>
#include <future>

namespace file_sorter {

std::string Archiver::getFormatDirName(FileFormat format) {
    switch (format) {
        case FileFormat::RAW: return "RAW";
        case FileFormat::JPEG: return "JPEG";
        case FileFormat::VIDEO: return "VIDEO";
        default: return "OTHERS";
    }
}

void Archiver::execute(const std::vector<EventGroup>& groups, 
                       const fs::path& targetRoot, 
                       ThreadPool& pool,
                       bool copyOnly) {
    
    std::vector<std::future<void>> futures;
    std::unordered_set<std::string> usedPaths;

    for (const auto& group : groups) {
        // 1. 构建目录名称：YYYYMMDD LocationName
        std::string folderName = group.dateString;
        if (!group.locationName.empty() && group.locationName != "Unknown_Location") {
            folderName += " " + group.locationName;
        }

        std::string yearStr = group.dateString.length() >= 4 ? group.dateString.substr(0, 4) : "Unknown_Year";
        fs::path groupPath = targetRoot / yearStr / folderName;

        for (const auto& photo : group.photos) {
            // 2. 构建最终物理路径：.../YYYYMMDD Location/RAW/filename.arw
            fs::path finalDir = groupPath / getFormatDirName(photo.format);
            
            // 确保目标目录存在 (在主线程同步创建，避免并发冲突)
            if (!fs::exists(finalDir)) {
                fs::create_directories(finalDir);
            }

            fs::path targetPath = finalDir / photo.originalPath.filename();

            // 3. 处理重名冲突 (如果目标已存在或在本次运行中被占用，则追加数字后缀)
            int counter = 1;
            while (fs::exists(targetPath) || usedPaths.count(targetPath.string())) {
                std::string newName = photo.originalPath.stem().string() 
                                    + "_" + std::to_string(counter) 
                                    + photo.originalPath.extension().string();
                targetPath = finalDir / newName;
                counter++;
            }
            usedPaths.insert(targetPath.string());

            // 4. 将实际的文件 I/O 操作扔进线程池
            futures.push_back(pool.enqueue([photo, targetPath, folderName, yearStr, copyOnly]() {
                try {
                    if (copyOnly) {
                        fs::copy_file(photo.originalPath, targetPath, fs::copy_options::overwrite_existing);
                    } else {
                        fs::rename(photo.originalPath, targetPath);
                    }

                    // spdlog 内部维护了线程安全，这里可以直接输出，无需额外锁
                    FS_INFO("{} {} -> {}/{}", 
                            (copyOnly ? "[COPY]" : "[MOVE]"), 
                            photo.originalPath.filename().string(), 
                            yearStr,
                            folderName);
                } catch (const fs::filesystem_error& e) {
                    FS_ERROR("File system error on {}: {}", photo.originalPath.filename().string(), e.what());
                }
            }));
        }
    }

    // 等待所有归档任务完成
    for (auto& f : futures) {
        f.get();
    }
}

} // namespace file_sorter