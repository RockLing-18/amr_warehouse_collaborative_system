#pragma once
#include <string>
#include <memory>

namespace edge_server
{
class ConfigManager;

class BootstrapService
{
public:
    BootstrapService();
    void init(const std::shared_ptr<ConfigManager>& configManager);
    std::string getBootstrap(const std::string& robot_id);

private:
    std::shared_ptr<ConfigManager> m_configManager;
};


}