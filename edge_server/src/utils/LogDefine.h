#pragma once
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/spdlog-inl.h"
#include "spdlog/sinks/stdout_color_sinks.h" 
#include <filesystem>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "utils/CommonFunc.h"

// 简单封装宏，自动使用默认 logger
#define LOG_TRACE(...)    SPDLOG_LOGGER_TRACE(spdlog::default_logger(), __VA_ARGS__)
#define LOG_DEBUG(...)    SPDLOG_LOGGER_DEBUG(spdlog::default_logger(), __VA_ARGS__)
#define LOG_INFO(...)     SPDLOG_LOGGER_INFO(spdlog::default_logger(), __VA_ARGS__)
#define LOG_WARN(...)     SPDLOG_LOGGER_WARN(spdlog::default_logger(), __VA_ARGS__)
#define LOG_ERROR(...)    SPDLOG_LOGGER_ERROR(spdlog::default_logger(), __VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(spdlog::default_logger(), __VA_ARGS__)

// #define LOG_TRACE(...)    SPDLOG_LOGGER_TRACE(spdlog::default_logger(), __FILE__, __LINE__, __VA_ARGS__)
// #define LOG_DEBUG(...)    SPDLOG_LOGGER_DEBUG(spdlog::default_logger(), __FILE__, __LINE__, __VA_ARGS__)
// #define LOG_INFO(...)     SPDLOG_LOGGER_INFO(spdlog::default_logger(), __FILE__, __LINE__, __VA_ARGS__)
// #define LOG_WARN(...)     SPDLOG_LOGGER_WARN(spdlog::default_logger(), __FILE__, __LINE__, __VA_ARGS__)
// #define LOG_ERROR(...)    SPDLOG_LOGGER_ERROR(spdlog::default_logger(), __FILE__, __LINE__, __VA_ARGS__)
// #define LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(spdlog::default_logger(), __FILE__, __LINE__, __VA_ARGS__)

inline spdlog::level::level_enum string_to_log_level(const std::string& str)
{
    if(str == "trace") return spdlog::level::trace;
    if(str == "debug") return spdlog::level::debug;
    if(str == "info")  return spdlog::level::info;
    if(str == "warn")  return spdlog::level::warn;
    if(str == "error") return spdlog::level::err;
    return spdlog::level::info;
}

class Log
{
public:
    static void init_console()
    {
        auto logger = spdlog::get("default_log");
        if(logger) return;

        auto bin_dir = utils::get_exe_dir();
        std::filesystem::create_directories(bin_dir + "/logs");

        // 固定启动日志文件，不受yaml控制，捕获启动阶段所有错误
        auto startup_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            bin_dir + "/logs/startup_err.log", 5*1024*1024, 3);
        startup_sink->set_level(spdlog::level::trace);

        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::trace);

        std::vector<spdlog::sink_ptr> sinks {startup_sink, console_sink};
        logger = std::make_shared<spdlog::logger>("default_log", sinks.begin(), sinks.end());
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [thread %t] %v");
        logger->set_level(spdlog::level::trace);
        logger->flush_on(spdlog::level::err);
        spdlog::register_logger(logger);
        spdlog::set_default_logger(logger);
    }


    static void init_logger(const std::string& logLevel)
    {
        //spdlog::drop("default_log");

        auto bin_dir = utils::get_exe_dir();
        std::filesystem::create_directories(bin_dir + "/logs");
        
        auto log_level = string_to_log_level(logLevel);
        auto logger = spdlog::get("file");
        if (!logger)
        {
            // 1. 日志文件 sink，每天凌晨 0 点生成新日志
            auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
               bin_dir + "/logs/app.log", 0, 0); // 0点0分生成新日志

            
            file_sink->set_level(log_level);
            // file_sink->set_level(spdlog::level::trace);

            // 2. 控制台 sink
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(log_level);

            // 3. 组合 sinks
            std::vector<spdlog::sink_ptr> sinks {file_sink, console_sink};
            logger = std::make_shared<spdlog::logger>("file", sinks.begin(), sinks.end());

            // 4. 日志格式 + flush 设置
            logger->set_level(log_level);
            logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%# %!] [thread %t] %v");
            logger->flush_on(log_level);

            spdlog::register_logger(logger);
        }

        // 设置默认 logger
        spdlog::set_default_logger(logger);
        spdlog::set_level(log_level);
    }

     static void init_logger2()
    {
        std::filesystem::create_directories("logs");

        auto logger = spdlog::get("file");

        if (!logger)
        {
            std::string log_file = "logs/app_" + utils::get_date_string() + ".log";
            // 1. 文件轮转 sink
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                log_file, 1024 * 1024 * 100, 20);
            file_sink->set_level(spdlog::level::trace);

            // 2. 控制台 sink
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::trace);

            // 3. 组合 sinks
            std::vector<spdlog::sink_ptr> sinks {file_sink, console_sink};
            logger = std::make_shared<spdlog::logger>("file", sinks.begin(), sinks.end());

            logger->set_level(spdlog::level::trace);
            logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%# %!] [thread %t] %v");
            logger->flush_on(spdlog::level::trace);

            spdlog::register_logger(logger);
        }

        spdlog::set_default_logger(logger);
    }

    static void clean_old_logs(int keep_days = 15)
    {
        namespace fs = std::filesystem;
        auto bin_dir = utils::get_exe_dir();
        fs::path log_path = bin_dir + "/logs";
        if(!fs::exists(log_path)) return;
        auto now = fs::file_time_type::clock::now();
        for (const auto& entry : fs::directory_iterator(log_path))
        {
            if (!entry.is_regular_file()) continue;
            auto age = std::chrono::duration_cast<std::chrono::hours>(now - entry.last_write_time()).count();
            if (age > keep_days * 24)
            {
                fs::remove(entry.path());
            }
        }
    }
};