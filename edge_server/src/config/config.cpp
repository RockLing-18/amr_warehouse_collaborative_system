#include "config/config.h"
#include "utils/LogDefine.h"
#include <yaml-cpp/yaml.h>
#include "mqtt/mqtt_topic.h"

namespace edge_server
{
bool ConfigManager::load(const std::string& file)
{
    try
    {
        auto yaml = YAML::LoadFile(file);
        m_config.websocket.host = yaml["websocket"]["host"].as<std::string>();
        m_config.websocket.port = yaml["websocket"]["port"].as<int>();
        m_config.websocket.protocol = yaml["websocket"]["protocol"].as<std::string>();
        m_config.robot.list_period_ms =  yaml["robot"]["list_period_ms"].as<int>();
        m_config.log.level =  yaml["log"]["level"].as<std::string>();

        std::string mqttIp = yaml["mqtt"]["host"].as<std::string>();
        int mqttPort = yaml["mqtt"]["port"].as<int>();
        m_config.edge_amr_mqtt.tls_enable = yaml["mqtt"]["tls_enable"].as<bool>();
        if(m_config.edge_amr_mqtt.tls_enable)
            m_config.edge_amr_mqtt.url =  "ssl://" + mqttIp + ":" + std::to_string(mqttPort);
        else
            m_config.edge_amr_mqtt.url =  "tcp://" + mqttIp + ":" + std::to_string(mqttPort);

        m_config.edge_amr_mqtt.user = yaml["mqtt"]["username"].as<std::string>();
        m_config.edge_amr_mqtt.pwd = yaml["mqtt"]["password"].as<std::string>();
        m_config.edge_amr_mqtt.client_id = yaml["mqtt"]["client_id"].as<std::string>();
        m_config.edge_amr_mqtt.ca_file = yaml["mqtt"]["ca_file"].as<std::string>();
        m_config.edge_amr_mqtt.keepalive = yaml["mqtt"]["keepalive"].as<int>();
        m_config.edge_amr_mqtt.reconnect_interval = yaml["mqtt"]["reconnect_interval"].as<int>();
        m_config.edge_amr_mqtt.will_msg_enable = true;
        m_config.edge_amr_mqtt.will.topic = mqtt_topic::EDGE_SERVER_STATUS;
        m_config.edge_amr_mqtt.will.payload = R"({"status":"offline"})";


        m_config.http.host = yaml["http"]["host"].as<std::string>();
        m_config.http.port = yaml["http"]["port"].as<int>();

        return true;
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("load yaml failed:{}", e.what());
        return false;
    }
}

Config ConfigManager::getConfig() const
{
    return m_config;
}


}