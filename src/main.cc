#include <iostream>
#include <string>
#include <algorithm>
#include "file_sorter/scanner.hpp"
#include "file_sorter/metadata_extractor.hpp"
#include "file_sorter/event_cluster.hpp"
#include "file_sorter/geo_resolver.hpp"
#include "file_sorter/archiver.hpp"
#include "file_sorter/thread_pool.hpp"
#include "file_sorter/logger.hpp"
#include "file_sorter/config.hpp"

using namespace file_sorter;

void runFixMode(const fs::path& targetDir) {
    FS_INFO("Entering interactive fix mode for unknown locations...");
    
    if (!fs::exists(targetDir) || !fs::is_directory(targetDir)) {
        FS_ERROR("Target directory does not exist: {}", targetDir.string());
        return;
    }

    bool fixedAny = false;
    std::vector<fs::path> dirsToCheck;

    for (const auto& entry : fs::directory_iterator(targetDir)) {
        if (entry.is_directory()) {
            dirsToCheck.push_back(entry.path());
            for (const auto& subEntry : fs::directory_iterator(entry.path())) {
                if (subEntry.is_directory()) {
                    dirsToCheck.push_back(subEntry.path());
                }
            }
        }
    }

    for (const auto& dirPath : dirsToCheck) {
        std::string folderName = dirPath.filename().string();
        
        // 简单匹配 YYYYMMDD 的文件夹（长度为8，且全为数字）
        if (folderName.length() == 8 && std::all_of(folderName.begin(), folderName.end(), [](unsigned char c){ return std::isdigit(c); })) {
            std::cout << "\nFound unknown location folder: " << folderName << "\n";
            std::cout << "Please enter a location name (press Enter to skip, type 'exit' to quit): ";
            
            std::string locationName;
            // 处理 EOF (Ctrl+D / Ctrl+Z) 或输入流异常
            if (!std::getline(std::cin, locationName)) {
                std::cout << "\nInput stream closed. Exiting fix mode...\n";
                break; 
            }

            // 去除首尾空格
            if (!locationName.empty()) {
                size_t first = locationName.find_first_not_of(" \t");
                if (first != std::string::npos) {
                    locationName.erase(0, first);
                    size_t last = locationName.find_last_not_of(" \t");
                    locationName.erase(last + 1);
                } else {
                    locationName.clear();
                }
            }

            // 处理用户主动输入的退出指令
            if (locationName == "exit" || locationName == "quit") {
                FS_INFO("User aborted fix mode.");
                break;
            }

            // 去除首尾空格
            if (!locationName.empty()) {
                size_t first = locationName.find_first_not_of(" \t");
                if (first != std::string::npos) {
                    locationName.erase(0, first);
                    size_t last = locationName.find_last_not_of(" \t");
                    locationName.erase(last + 1);
                } else {
                    locationName.clear(); // all spaces
                }
            }

            if (!locationName.empty()) {
                std::string newFolderName = folderName + " " + locationName;
                fs::path newPath = dirPath.parent_path() / newFolderName;
                
                try {
                    fs::rename(dirPath, newPath);
                    FS_INFO("Renamed to: {}", newFolderName);
                    fixedAny = true;
                } catch (const fs::filesystem_error& e) {
                    FS_ERROR("Failed to rename: {}", e.what());
                }
            } else {
                FS_INFO("Skipped.");
            }
        }
    }

    if (!fixedAny) {
        FS_INFO("No folders needed fixing, or all were skipped.");
    } else {
        FS_INFO("Fix mode complete.");
    }
}

void cleanupEmptyDirectories(const fs::path& dir) {
    std::vector<fs::path> dirs;
    try {
        if (!fs::exists(dir)) return;
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (entry.is_directory()) {
                dirs.push_back(entry.path());
            }
        }
    } catch (const fs::filesystem_error& e) {
        FS_WARN("Error scanning for empty directories: {}", e.what());
        return;
    }

    // reverse so we process deeper directories first (children before parents)
    std::reverse(dirs.begin(), dirs.end());

    int removedCount = 0;
    for (const auto& d : dirs) {
        try {
            if (fs::exists(d) && fs::is_empty(d)) {
                fs::remove(d);
                removedCount++;
            }
        } catch (...) {
            // Ignore errors (like permissions) during cleanup
        }
    }

    if (removedCount > 0) {
        FS_INFO("Cleaned up {} empty directories in source.", removedCount);
    }
}

int main(int argc, char* argv[]) {
    // 1. Load config first
    Config::getInstance().load("config.yaml");

    // 2. Initialize logger (which now uses log path from config)
    Logger::init(); 

    fs::path sourceDir = Config::getInstance().source_dir;
    fs::path targetDir = Config::getInstance().target_dir;

    if (argc > 1 && (std::string(argv[1]) == "--fix" || std::string(argv[1]) == "-f")) {
        runFixMode(targetDir);
        return 0;
    }

    if (sourceDir.empty() || targetDir.empty()) {
        FS_ERROR("source_dir or target_dir is not specified in config.yaml");
        return 1;
    }

    // 1. 扫描文件
    FS_INFO("Step 1: Scanning directory...");
    auto paths = Scanner::scanDirectory(sourceDir);
    
    // 增加短路逻辑：如果目录为空，直接静默退出
    if (paths.empty()) {
        FS_INFO("No valid media files found in source directory. Exiting.");
        spdlog::shutdown();
        return 0;
    }
    
    // 2. 提取元数据
    FS_INFO("Step 2: Extracting metadata...");
    std::vector<PhotoMetadata> allMeta;
    std::vector<std::future<PhotoMetadata>> futures;

    allMeta.reserve(paths.size());
    futures.reserve(paths.size());

    // 优先使用配置文件中的线程数，若为0则动态获取硬件支持的并发线程数
    unsigned int threadCount = Config::getInstance().thread_count;
    if (threadCount == 0) {
        threadCount = std::thread::hardware_concurrency();
        if (threadCount == 0) threadCount = 4; // 兜底
    }
    
    ThreadPool pool(threadCount);
    FS_INFO("Using {} threads for processing.", threadCount);

    for (const auto& p : paths) {
        // 先判断格式，避免在 enqueue 内部重复调用
        FileFormat format = Scanner::determineFormat(p);
        
        // 将解析任务扔进线程池，保存返回的 future
        futures.push_back(pool.enqueue(MetadataExtractor::extract, p, format));
    }

    // 阻塞等待所有任务完成，并收集结果
    for (auto& f : futures) {
        allMeta.push_back(f.get());
    }

    // 3. 时间聚类与中位数坐标计算
    FS_INFO("Step 3: Clustering photos by time...");
    auto groups = EventCluster::clusterPhotos(allMeta);

    // 4. 地理位置解析
    FS_INFO("Step 4: Resolving locations and caching...");
    GeoResolver::initCache(Config::getInstance().geo_cache_path); 
    for (auto& group : groups) {
        if (group.medianLocation.isValid) {
            group.locationName = GeoResolver::resolve(group.medianLocation);
        } else {
            group.locationName = "Unknown_Location";
        }
    }
    GeoResolver::saveCache();

    // 5. 物理归档
    FS_INFO("Step 5: Executing archival...");
    bool copyMode = Config::getInstance().copy_only;
    Archiver::execute(groups, targetDir, pool, copyMode);

    // 6. 删除空目录
    if (!copyMode) {
        FS_INFO("Step 6: Cleaning up empty source directories...");
        cleanupEmptyDirectories(sourceDir);
    }

    FS_INFO("Done! All files sorted successfully.");
    spdlog::shutdown();
    return 0;
}