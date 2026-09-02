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

struct Config
{
    WebSocket websocket;
    Robot robot;
    LogCfg log;
};


class ConfigLoader
{
public:
    static bool load(const std::string& file, Config& config);
};


}