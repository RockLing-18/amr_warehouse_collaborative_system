#pragma once

#include <string>
#include <chrono>

namespace simulation_manager
{

enum class RobotState
{
    UNKNOWN,
    ACTIVE,          // Gazebo存在，并且控制器ready
    SPAWNING,        // 已经提交spawn
    SPAWNED,         // Gazebo模型出现
    ERROR,
    STOPPING
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
    std::chrono::steady_clock::time_point update_time;
    pid_t launch_pid{-1};
};

}