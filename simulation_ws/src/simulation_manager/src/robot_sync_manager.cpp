#include "simulation_manager/robot_sync_manager.h"
#include <algorithm>

namespace simulation_manager
{
RobotSyncManager::RobotSyncManager(const rclcpp::Node::SharedPtr& node, const std::shared_ptr<GazeboClient>& gazebo_client, const std::shared_ptr<RobotLifecycleManager>& lifecycle)
: m_node(node), m_gazebo_client(gazebo_client),  m_lifecycle_manager(lifecycle)
{
}

RobotSyncManager::~RobotSyncManager()
{
}

void RobotSyncManager::sync(const std::vector<RobotInfo>& edge_robots)
{
    const std::vector<GazeboModelInfo>& gazebo_models = m_gazebo_client->getModels();
    for (const auto& robot : edge_robots)
    {
        auto it = std::find_if(
            gazebo_models.begin(),
            gazebo_models.end(),
            [&](const GazeboModelInfo& model)
            {
                return model.robot_id == robot.robot_id;
            });

        if (it == gazebo_models.end())
        {
            m_lifecycle_manager->requestCreate(robot);
            continue;
        }

        if (it->instance_id != robot.instance_id)
        {
            m_lifecycle_manager->requestReplace(robot, *it);
        }
    }

    // Gazebo -> Edge
    for (const auto& model : gazebo_models)
    {
        auto it = std::find_if(
            edge_robots.begin(),
            edge_robots.end(),
            [&](const RobotInfo& robot)
            {
                return robot.robot_id == model.robot_id;
            });

        if (it == edge_robots.end())
        {
            m_lifecycle_manager->requestDelete(model);
        }
    }
}

}
