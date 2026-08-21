#pragma once
#include "data_define.h"
#include <vector>

namespace simulation_manager
{

class EdgeClient
{
public:
    EdgeClient(const std::string &websocket_url);

    bool connect();

    bool isConnected() const;

    std::vector<RobotInfo> getRobots();

private:
    std::string m_websocket_url;
};

}