#pragma once
#include <rclcpp/rclcpp.hpp>
#include "simulation_manager/edge_client.h"
#include "simulation_manager/gazebo_client.h"
#include "simulation_manager/amr_process_manager.h"
#include "simulation_manager/robot_sync_manager.h"
#include "simulation_manager/robot_lifecycle_manager.h"

namespace simulation_manager
{

class SimulationManagerNode : public rclcpp::Node
{
public:
    explicit SimulationManagerNode();
    void init();

private:
    void onTimer();

private:
    std::shared_ptr<EdgeClient> m_edge_client;
    std::shared_ptr<GazeboClient> m_gazebo_client;
    std::shared_ptr<AmrProcessManager> m_process_manager;
    std::shared_ptr<RobotSyncManager> m_sync_manager;
    std::shared_ptr<RobotLifecycleManager> m_lifecycle_manager;
    rclcpp::TimerBase::SharedPtr m_timer;
};

}