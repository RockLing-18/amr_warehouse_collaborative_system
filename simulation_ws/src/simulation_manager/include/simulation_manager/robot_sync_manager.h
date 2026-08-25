#pragma once

#include "simulation_manager/data_define.h"
#include "simulation_manager/gazebo_client.h"
#include "simulation_manager/amr_process_manager.h"
#include "simulation_manager/robot_lifecycle_manager.h"


#include <memory>

namespace simulation_manager
{

class RobotSyncManager
{
public:
    RobotSyncManager(const rclcpp::Node::SharedPtr& node, const std::shared_ptr<GazeboClient>& gazebo_client, const std::shared_ptr<RobotLifecycleManager>& lifecycle);
    ~RobotSyncManager();    
    void sync(const std::vector<RobotInfo>& edge_robots);

private:
    rclcpp::Node::SharedPtr m_node;
    std::shared_ptr<GazeboClient> m_gazebo_client;
    std::shared_ptr<RobotLifecycleManager> m_lifecycle_manager;
};

}