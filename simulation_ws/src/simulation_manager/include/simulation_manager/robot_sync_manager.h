#pragma once

#include "simulation_manager/data_define.h"
#include "simulation_manager/gazebo_client.h"
#include "simulation_manager/amr_process_manager.h"
#include "simulation_manager/controller_checker.h"


#include <memory>
#include <unordered_map>
#include <thread>

namespace simulation_manager
{

class RobotSyncManager
{
public:
    RobotSyncManager(const rclcpp::Node::SharedPtr& node, const std::shared_ptr<GazeboClient>& gazebo_client, const std::shared_ptr<AmrProcessManager>& process_manager, const rclcpp::Logger& logger);
    void sync(const std::vector<RobotInfo>& edge_robots);

private:
    void syncWithGazebo(const std::vector<RobotInfo>& edge_robots, const std::vector<GazeboModelInfo>& gazebo_models);

    void addRobot(const RobotInfo& robot);

    void removeRobot(const GazeboModelInfo& model);

    void replaceRobot(const RobotInfo& robot, const GazeboModelInfo& model);

    void requestSpawn(const RobotInfo& robot);

    void spawnWorker();

    void checkControllerReady(ManagedRobot& robot);

    std::string makeKey(const std::string& robot_id, const std::string& instance_id) const;

private:
    rclcpp::Node::SharedPtr m_node;
    std::shared_ptr<GazeboClient> m_gazebo_client;
    std::shared_ptr<AmrProcessManager> m_process_manager;
    std::shared_ptr<ControllerChecker> m_controller_checker;
    std::unordered_map<std::string, ManagedRobot> m_robots;
    rclcpp::Logger m_logger;
    std::thread m_worker_thread;
};

}