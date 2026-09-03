#pragma once

#include <string>

namespace edge_server
{

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

};

struct Config
{
    WebSocket websocket;
    Robot robot;
    LogCfg log;
    MqttCfg mqtt;
};


class ConfigLoader
{
public:
    static bool load(const std::string& file, Config& config);
};


}