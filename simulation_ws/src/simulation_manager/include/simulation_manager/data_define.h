#pragma once

#include <string>
#include <chrono>

namespace simulation_manager
{

enum class RobotState
{
    UNKNOWN,
    WAITING_CREATE,
    LAUNCHING,
    WAIT_GAZEBO_MODEL,
    WAIT_CONTROLLER,
    ACTIVE,
    FAILED,
    DELETING
};


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

struct ManagedRobot
{
    RobotInfo info;
    RobotState state{RobotState::UNKNOWN};
};

}