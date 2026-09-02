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
        return true;
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("load yaml failed:{}", e.what());
        return false;
    }
}


}