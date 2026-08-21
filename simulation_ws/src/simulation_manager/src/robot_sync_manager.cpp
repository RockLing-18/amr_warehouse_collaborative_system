#include "simulation_manager/robot_sync_manager.h"

namespace simulation_manager
{
RobotSyncManager::RobotSyncManager(const std::shared_ptr<GazeboClient>& gazebo_client,  const std::shared_ptr<AmrProcessManager>& process_manager, const rclcpp::Logger& logger)
: m_gazebo_client(gazebo_client),  m_process_manager(process_manager), m_logger(logger)
{
}

void RobotSyncManager::sync(const std::vector<RobotInfo>& edge_robots)
{
    std::vector<GazeboModelInfo> gazebo_models;

    if (!m_gazebo_client->getModels(gazebo_models))
    {
        RCLCPP_WARN( m_logger, "Failed to get Gazebo models");
        return;
    }

    // --------------------------------------------------
    // Edge -> Gazebo
    // --------------------------------------------------
    for (const auto& robot : edge_robots)
    {
        auto it = std::find_if(gazebo_models.begin(), gazebo_models.end(), [&](const GazeboModelInfo& model)
            {
                return model.robot_id == robot.robot_id;
            });

        if (it == gazebo_models.end())
        {
            addRobot(robot);
            continue;
        }

        if (it->instance_id != robot.instance_id)
        {
            replaceRobot(robot, *it);
        }
    }

    // --------------------------------------------------
    // Gazebo -> Edge
    // --------------------------------------------------

    for (const auto& model : gazebo_models)
    {
        if (model.robot_id.empty())
            continue;

        auto it = std::find_if(edge_robots.begin(), edge_robots.end(), [&](const RobotInfo& robot)
            {
                return robot.robot_id == model.robot_id;
            });

        if (it == edge_robots.end())
        {
            removeRobot(model);
        }
    }
}

void RobotSyncManager::addRobot(const RobotInfo& robot)
{
    RCLCPP_INFO(
        m_logger,
        "Robot missing in Gazebo, spawn robot=%s instance=%s",
        robot.robot_id.c_str(),
        robot.instance_id.c_str());

    if (!m_process_manager->spawn(robot))
    {
        RCLCPP_ERROR(
            m_logger,
            "Failed to spawn robot=%s instance=%s",
            robot.robot_id.c_str(),
            robot.instance_id.c_str());

        return;
    }
}

void RobotSyncManager::removeRobot(const GazeboModelInfo& model)
{
    RCLCPP_INFO(
        m_logger,
        "Robot exists in Gazebo but not Edge: "
        "robot=%s instance=%s",
        model.robot_id.c_str(),
        model.instance_id.c_str());

    m_process_manager->stop(model.robot_id, model.instance_id);

    if (!m_gazebo_client->deleteModel(model.model_name))
        RCLCPP_ERROR( m_logger, "Failed to delete Gazebo model: %s", model.model_name.c_str());
    
}

void RobotSyncManager::replaceRobot(const RobotInfo& robot, const GazeboModelInfo& model)
{
    RCLCPP_INFO(
        m_logger,
        "Robot instance changed: "
        "robot=%s old=%s new=%s",
        robot.robot_id.c_str(),
        model.instance_id.c_str(),
        robot.instance_id.c_str());

    // 1. 停止旧 launch
    m_process_manager->stop(model.robot_id, model.instance_id);

    // 2. 删除旧 Gazebo model
    if (!m_gazebo_client->deleteModel(model.model_name))
    {
        RCLCPP_ERROR( m_logger, "Failed to delete old model: %s", model.model_name.c_str());
        return;
    }

    // 3. 创建新实例
    if (!m_process_manager->spawn(robot))
        RCLCPP_ERROR(m_logger, "Failed to spawn replacement robot: %s", robot.robot_id.c_str());
}

}