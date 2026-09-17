#pragma once 
#include <string>
#include "types.h"

namespace amr_agent
{

class Bootstrap
{
public:

    bool run(BootstrapInfo& info);


private:

    bool loadLocalConfig();

    bool requestBootstrap(BootstrapInfo& info);


private:
    std::string m_edge_host;
    int m_edge_port;
    std::string m_robot_id;
};

}