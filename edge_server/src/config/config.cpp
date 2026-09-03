#include "config/config.h"
#include "utils/LogDefine.h"
#include <yaml-cpp/yaml.h>

namespace edge_server
{
bool ConfigLoader::load(const std::string& file, Config& config)
{
    try
    {
        auto yaml = YAML::LoadFile(file);
        config.websocket.host = yaml["websocket"]["host"].as<std::string>();
        config.websocket.port = yaml["websocket"]["port"].as<int>();
        config.websocket.protocol = yaml["websocket"]["protocol"].as<std::string>();
        config.robot.list_period_ms =  yaml["robot"]["list_period_ms"].as<int>();
        config.log.level =  yaml["log"]["level"].as<std::string>();

        std::string mqttIp = yaml["mqtt"]["host"].as<std::string>();
        int mqttPort = yaml["mqtt"]["port"].as<int>();
        config.mqtt.tls_enable = yaml["mqtt"]["tls_enable"].as<bool>();
        if(config.mqtt.tls_enable)
            config.mqtt.url =  "ssl://" + mqttIp + ":" + std::to_string(mqttPort);
        else
            config.mqtt.url =  "tcp://" + mqttIp + ":" + std::to_string(mqttPort);

        config.mqtt.user = yaml["mqtt"]["username"].as<std::string>();
        config.mqtt.pwd = yaml["mqtt"]["password"].as<std::string>();
        config.mqtt.client_id = yaml["mqtt"]["client_id"].as<std::string>();
        config.mqtt.ca_file = yaml["mqtt"]["ca_file"].as<std::string>();
        config.mqtt.keepalive = yaml["mqtt"]["keepalive"].as<int>();
        config.mqtt.reconnect_interval = yaml["mqtt"]["reconnect_interval"].as<int>();

        return true;
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("load yaml failed:{}", e.what());
        return false;
    }
}


}