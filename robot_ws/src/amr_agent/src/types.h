#pragma once 
#include <string>

namespace amr_agent
{

struct MqttMessage
{
    std::string topic;
    std::string payload;
};

struct MqttConfig
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


struct MapConfig
{
    std::string path; // 根目录路径
    std::string version;
    std::string mapName; // 如: map.yaml
    std::string imageName; // 如: map.pgm
    std::string zoneName;  // 如: zone.yaml
    std::string stationName;   // 如: station.yaml
};

struct BootstrapInfo
{
    std::string robot_id;
    std::string warehouse_id;
    MqttConfig mqtt;
    MapConfig map;
};
}