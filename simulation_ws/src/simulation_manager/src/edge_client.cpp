#include "simulation_manager/edge_client.h"

namespace simulation_manager
{
EdgeClient::EdgeClient(const std::string &websocket_url):m_websocket_url(websocket_url)
{}

bool EdgeClient::connect()
{
    return true;
}

bool EdgeClient::isConnected() const
{
    return true;
}

std::vector<RobotInfo> EdgeClient::getRobots()
{
    std::vector<RobotInfo> robots = {
        {
            "robot01",
            "A001",
            2.0,
            3.0,
            1.57
        },
        {
            "robot02",
            "B001",
            0.0,
            0.0,
            0.0
        }
    };

    return robots;
}
}