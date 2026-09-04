#pragma once
#include <string>

namespace edge_server
{

struct RobotInfo
{
    std::string robot_id;
    std::string simulation_instance_id;
    std::string register_timestamp;  // 注册的时间戳, 暂时以其作为instance_id
    double x{0};
    double y{0};
    double yaw{0};
};

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


}