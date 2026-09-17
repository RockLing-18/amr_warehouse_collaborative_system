#pragma once 
#include <string>

namespace amr_agent
{

struct MqttConfig
{
    std::string host;
    int port;
    std::string username;
    std::string password;
};


struct MapConfig
{
    std::string version;
};

struct BootstrapInfo
{
    std::string robot_id;
    std::string warehouse_id;
    MqttConfig mqtt;
    MapConfig map;
};
}