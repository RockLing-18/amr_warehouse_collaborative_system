#pragma once

#include "simulation_manager/data_define.h"
#include "simulation_manager/gazebo_client.h"
#include "simulation_manager/amr_process_manager.h"

#include <memory>

namespace simulation_manager
{

class RobotSyncManager
{
public:
    RobotSyncManager(const std::shared_ptr<GazeboClient>& gazebo_client, const std::shared_ptr<AmrProcessManager>& process_manager, const rclcpp::Logger& logger);
    void sync(const std::vector<RobotInfo>& edge_robots);

private:
    void syncWithGazebo(const std::vector<RobotInfo>& edge_robots, const std::vector<GazeboModelInfo>& gazebo_models);

    void addRobot(const RobotInfo& robot);

    void removeRobot(const GazeboModelInfo& model);

    void replaceRobot(const RobotInfo& robot, const GazeboModelInfo& model);

private:
    std::shared_ptr<GazeboClient> m_gazebo_client;
    std::shared_ptr<AmrProcessManager> m_process_manager;
    rclcpp::Logger m_logger;
};

}