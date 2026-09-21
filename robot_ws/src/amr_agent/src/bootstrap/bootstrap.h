#pragma once
#include <string>
#include "types.h"

namespace amr_agent
{

class Bootstrap
{
public:
    bool run(const std::string& cfgPath, BootstrapInfo& info);

private:
    bool loadLocalConfig(const std::string& cfgPath, BootstrapInfo& info);
    bool requestBootstrap( BootstrapInfo& info);
    bool loadMapMetadata(BootstrapInfo& info);

private:
    std::string m_robotId;
    std::string m_edgeHost;
    int m_edgePort = 8080;
};

}