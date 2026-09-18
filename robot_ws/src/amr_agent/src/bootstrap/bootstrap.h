#pragma once 
#include <rclcpp/rclcpp.hpp>
#include <string>
#include "types.h"

namespace amr_agent
{

class Bootstrap
{
public:
    bool run(const rclcpp::Logger& logger, const std::string& cfgPath, BootstrapInfo& info);

private:
    bool loadLocalConfig(const rclcpp::Logger& logger, const std::string& cfgPath, BootstrapInfo& info);
    bool requestBootstrap(const rclcpp::Logger& logger, BootstrapInfo& info);
    bool loadMapMetadata(const rclcpp::Logger& logger, BootstrapInfo& info);

private:
    std::string m_robotId;
    std::string m_edgeHost;
    int m_edgePort = 8080;
};

}