#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>

namespace file_sorter {

class Logger {
public:
    static void init();
    static std::shared_ptr<spdlog::logger>& getLogger();

private:
    static std::shared_ptr<spdlog::logger> s_logger;
};

} // namespace file_sorter

// 便捷调用的宏定义
#define FS_TRACE(...)    ::file_sorter::Logger::getLogger()->trace(__VA_ARGS__)
#define FS_INFO(...)     ::file_sorter::Logger::getLogger()->info(__VA_ARGS__)
#define FS_WARN(...)     ::file_sorter::Logger::getLogger()->warn(__VA_ARGS__)
#define FS_ERROR(...)    ::file_sorter::Logger::getLogger()->error(__VA_ARGS__)
#define FS_CRITICAL(...) ::file_sorter::Logger::getLogger()->critical(__VA_ARGS__)