#include "file_sorter/logger.hpp"
#include "file_sorter/config.hpp"
#include <vector>

namespace file_sorter {

std::shared_ptr<spdlog::logger> Logger::s_logger;

void Logger::init() {
    // 1. 控制台彩色输出 Sink (级别：INFO 及以上)
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::info);
    console_sink->set_pattern("[%^%l%$] %v"); // 简洁格式：[INFO] 消息内容

    // 2. 文件输出 Sink (级别：TRACE 及以上)，支持追加写入，存放在 config 指定目录下
    // 使用 daily_file_sink，按天生成日志文件，不限制文件总数。
    // 这可以永久保留所有历史日志，文件名会自动附加日期（如 file_sorter-2024-03-22.log）。
    std::string logFilePath = Config::getInstance().log_path;
    // 参数：日志基础文件名，轮换时间（小时），轮换时间（分钟）。这里设置为每天 0 点 0 分。
    auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(logFilePath, 0, 0);

    file_sink->set_level(spdlog::level::trace);
    // 详细格式：[时间] [级别] [线程ID] 消息内容
    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [thread %t] %v"); 

    std::vector<spdlog::sink_ptr> sinks {console_sink, file_sink};

    // 3. 组合 Sink 生成核心 logger
    s_logger = std::make_shared<spdlog::logger>("FileSorter", sinks.begin(), sinks.end());
    s_logger->set_level(spdlog::level::trace); // 全局日志级别阈值放宽到 TRACE
    s_logger->flush_on(spdlog::level::info);   // 遇到 INFO 及以上级别的日志立刻刷新缓冲区

    spdlog::register_logger(s_logger);
}

std::shared_ptr<spdlog::logger>& Logger::getLogger() {
    return s_logger;
}

} // namespace file_sorter