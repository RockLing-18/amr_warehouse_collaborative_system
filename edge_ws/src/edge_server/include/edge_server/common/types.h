#pragma once
#include <string>

namespace edge_server
{

struct RobotInfo
{
    std::string robot_id;
    std::string instance_id;
    double x{0};
    double y{0};
    double yaw{0};
};

}