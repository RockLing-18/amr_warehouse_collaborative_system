#include "service/bootstrap_service.h"
#include "nlohmann/json.hpp"
#include "config/config.h"


namespace edge_server
{

using json=nlohmann::json;


BootstrapService::BootstrapService()
{
}

void BootstrapService::init(const std::shared_ptr<ConfigManager>& configManager)
{
    m_configManager = configManager;
}


std::string BootstrapService::getBootstrap(const std::string& robot_id)
{
    const auto& config = m_configManager->getConfig();
    json root;
    root["code"] = 0;
    root["data"]["warehouse_id"] = config.warehouse.id;
    root["data"]["mqtt"]["url"] = config.edge_amr_mqtt.url;
    root["data"]["mqtt"]["client_id"] = robot_id;
    root["data"]["mqtt"]["username"] = config.edge_amr_mqtt.user;
    root["data"]["mqtt"]["password"] = config.edge_amr_mqtt.pwd;
    root["data"]["mqtt"]["tls_enable"] = config.edge_amr_mqtt.tls_enable;
    root["data"]["mqtt"]["keepalive"] = config.edge_amr_mqtt.keepalive;
    root["data"]["mqtt"]["reconnect_interval"] = config.edge_amr_mqtt.reconnect_interval;
   // root["data"]["mqtt"]["ca_file_url"] = config.edge_amr_mqtt.reconnect_interval;

    return root.dump();
}


}