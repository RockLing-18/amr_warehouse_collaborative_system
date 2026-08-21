#pragma once

#include <string>

namespace simulation_manager
{

struct RobotInfo
{
    std::string robot_id;
    std::string instance_id;

    double x{0.0};
    double y{0.0};
    double yaw{0.0};
};

struct GazeboModelInfo
{
    std::string model_name;
    std::string robot_id;
    std::string instance_id;
};

}