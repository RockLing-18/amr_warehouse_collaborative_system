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

struct Config
{
    WebSocket websocket;
    Robot robot;
    LogCfg log;
    MqttCfg edge_amr_mqtt;
};

enum class RobotState
{
    UNKNOWN = 0,
    IDLE,          // 空闲，可接任务
    WORKING,       // 执行任务中
    CHARGING,      // 充电
    PAUSED,        // 暂停
    ERROR,         // 故障
};

// 机器人位姿
struct RobotPose
{
    double x;
    double y;
    double yaw;
};

struct RobotInfo
{
    std::string robot_id;
    std::string simulation_instance_id;
    std::string register_timestamp;  // 注册的时间戳, 暂时以其作为instance_id
    RobotPose pose;
};

struct RobotStatus
{
    std::string robot_id;
    RobotState state;
    bool online;
    uint64_t timestamp;
    double battery;
    RobotPose pose;
    std::string task_id;
    int error_code;
};

}