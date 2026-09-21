#pragma once

#include <rclcpp/rclcpp.hpp>
#include <string>

namespace amr_agent
{
class GlobalRosLogger
{
public:
    //由ROS Node在初始化时调用，注入根logger
    static void init(rclcpp::Logger root_logger, rclcpp::Logger::Level level = rclcpp::Logger::Level::Info)
    {
        logger_ = std::move(root_logger);
        logger_.set_level(level);
    }

    //获取全局logger引用
    static rclcpp::Logger& get()
    {
        return logger_;
    }

private:
    // 私有构造，禁止实例化
    GlobalRosLogger() = default;
    static inline rclcpp::Logger logger_{rclcpp::get_logger("global_default")};
};

// 日志宏定义，业务层直接使用
#define LOG_INFO(fmt, ...)    RCLCPP_INFO(amr_agent::GlobalRosLogger::get(), fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)    RCLCPP_WARN(amr_agent::GlobalRosLogger::get(), fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)   RCLCPP_ERROR(amr_agent::GlobalRosLogger::get(), fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)   RCLCPP_DEBUG(amr_agent::GlobalRosLogger::get(), fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...)   RCLCPP_FATAL(amr_agent::GlobalRosLogger::get(), fmt, ##__VA_ARGS__)

// 节流版本（可选，高频日志防刷屏）
#define LOG_INFO_THROTTLE(clock, period, fmt, ...) RCLCPP_INFO_THROTTLE(amr_agent::GlobalRosLogger::get(), clock, period, fmt, ##__VA_ARGS__)
#define LOG_ERROR_THROTTLE(clock, period, fmt, ...) RCLCPP_ERROR_THROTTLE(amr_agent::GlobalRosLogger::get(), clock, period, fmt, ##__VA_ARGS__)



// 每个业务组件独立 logger，日志带模块名，rqt 可以按模块过滤；无全局单例
class ModuleRosLogger
{
public:
    explicit ModuleRosLogger(rclcpp::Logger logger)
        : logger_(std::move(logger))
    {}

    template<typename... Args>
    void info(const char* fmt, Args&&... args)
    {
        RCLCPP_INFO(logger_, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void warn(const char* fmt, Args&&... args)
    {
        RCLCPP_WARN(logger_, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error(const char* fmt, Args&&... args)
    {
        RCLCPP_ERROR(logger_, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void debug(const char* fmt, Args&&... args)
    {
        RCLCPP_DEBUG(logger_, fmt, std::forward<Args>(args)...);
    }

    // 获取底层原生logger，供RCLCPP_THROTTLE等宏使用
    rclcpp::Logger get_native_logger() const
    {
        return logger_;
    }

private:
    rclcpp::Logger logger_;
};

// 配套宏（可选，方便迁移旧代码）
#define MOD_LOG_INFO(logger, fmt, ...)  RCLCPP_INFO((logger).get_native_logger(), fmt, ##__VA_ARGS__)
#define MOD_LOG_ERROR(logger, fmt, ...) RCLCPP_ERROR((logger).get_native_logger(), fmt, ##__VA_ARGS__)


/*
auto mqtt_logger = edge_server::ModuleRosLogger(this->get_logger().get_child("mqtt_client"));
auto map_logger = edge_server::ModuleRosLogger(this->get_logger().get_child("map_service"));

auto mqtt_client = std::make_shared<MqttClient>(mqtt_logger);
auto map_service = std::make_shared<MapService>(map_logger);


class MqttClient
{
public:
    explicit MqttClient(edge_server::ModuleRosLogger logger)
        : logger_(std::move(logger))
    {}

    void connect(const std::string& ip)
    {
        logger_.info("mqtt try connect, ip: %s", ip.c_str());
        logger_.error("mqtt connect failed");
    }

private:
    ModuleRosLogger logger_;
};



*/

}
