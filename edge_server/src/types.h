#pragma once
#include <string>

namespace edge_server
{

struct MqttMessage
{
    std::string topic;
    std::string payload;
};

struct WebSocket
{
    std::string host;
    int port;
    std::string protocol;
};

struct Robot
{
    int list_period_ms;
};

struct LogCfg
{
    std::string level;
};

struct MqttCfg
{
    std::string url;
    std::string client_id;
    std::string user;
    std::string pwd;
    std::string ca_file;
    int keepalive;
    int reconnect_interval;
    bool tls_enable{false};
    bool will_msg_enable{false};
    MqttMessage will;
};

struct HttpCfg
{
    std::string host;
    int port;
    std::string ca_file;
    bool tls_enable{false};
};

struct Config
{
    WebSocket websocket;
    Robot robot;
    LogCfg log;
    MqttCfg edge_amr_mqtt;
    HttpCfg http;
};

enum class RobotState
{
    OFFLINE = 0,
    INITIALIZING,  // 正在初始化,不可接任务
    IDLE,          // 空闲，可接任务
    WORKING,       // 执行任务中
    CHARGING,      // 充电
    PAUSED,        // 暂停
    ERROR,         // 故障
};

// 机器人位姿
struct Pose2D
{
    double x;
    double y;
    double yaw;
};

struct RobotBaseInfo
{
    std::string robot_id;
    std::string simulation_instance_id;
    uint64_t register_timestamp;  // 注册的时间戳, 暂时以其作为instance_id
};

struct RobotRunningStatus
{
    RobotState state;
    bool online;
    uint64_t timestamp;
    double battery;
    Pose2D pose;
    std::string task_id;
};

struct RobotInstanceInfo
{
    std::string robot_id;
    std::string simulation_instance_id;
    Pose2D pose;
};

// 字符串 -> 枚举（MQTT收到消息时调用）
inline RobotState RobotStateFromString(const std::string& str)
{
    if(str == "OFFLINE") return RobotState::OFFLINE;
    if(str == "INITIALIZING") return RobotState::INITIALIZING;
    if(str == "IDLE") return RobotState::IDLE;
    if(str == "WORKING") return RobotState::WORKING;
    if(str == "CHARGING") return RobotState::CHARGING;
    if(str == "PAUSED") return RobotState::PAUSED;
    if(str == "ERROR") return RobotState::ERROR;
    return RobotState::OFFLINE; // 非法字符串兜底
}

// 枚举 -> 字符串（上报MQTT、打印日志）
inline std::string RobotStateToString(RobotState s)
{
    switch(s)
    {
        case RobotState::OFFLINE: return "UNKOFFLINENOWN";
        case RobotState::INITIALIZING: return "INITIALIZING";
        case RobotState::IDLE: return "IDLE";
        case RobotState::WORKING: return "WORKING";
        case RobotState::CHARGING: return "CHARGING";
        case RobotState::PAUSED: return "PAUSED";
        case RobotState::ERROR: return "ERROR";
        default: return "OFFLINE";
    }
}

}