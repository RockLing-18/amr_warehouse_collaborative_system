#include "simulation_manager/robot_sync_manager.h"

namespace simulation_manager
{
RobotSyncManager::RobotSyncManager(const std::shared_ptr<GazeboClient>& gazebo_client,  const std::shared_ptr<AmrProcessManager>& process_manager, const rclcpp::Logger& logger)
: m_gazebo_client(gazebo_client),  m_process_manager(process_manager), m_logger(logger)
{
}

void RobotSyncManager::sync(const std::vector<RobotInfo>& edge_robots)
{
    m_gazebo_client->getModelsAsync(
        [this, edge_robots](const std::vector<GazeboModelInfo>& gazebo_models)
        {
            syncWithGazebo(
                edge_robots,
                gazebo_models);
        });
}

void RobotSyncManager::syncWithGazebo(const std::vector<RobotInfo>& edge_robots, const std::vector<GazeboModelInfo>& gazebo_models)
{
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
            addRobot(robot);
            std::this_thread::sleep_for(std::chrono::seconds(6));
            continue;
        }

        if (it->instance_id != robot.instance_id)
        {
            replaceRobot(robot, *it);
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

     m_gazebo_client->deleteModelAsync(
        model.model_name,
        [this, model](bool success)
        {
            if (!success)
            {
                RCLCPP_ERROR(m_logger, "Failed to delete Gazebo model: %s", model.model_name.c_str());
                return;
            }

            RCLCPP_INFO(
                m_logger,
                "Robot removed successfully: "
                "robot=%s instance=%s",
                model.robot_id.c_str(),
                model.instance_id.c_str());
        });

    // if (!m_gazebo_client->deleteModel(model.model_name))
    //     RCLCPP_ERROR( m_logger, "Failed to delete Gazebo model: %s", model.model_name.c_str());
    
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

    // 2. 异步删除旧 Gazebo model
    m_gazebo_client->deleteModelAsync(
        model.model_name,
        [this, robot, model](bool success)
        {
            if (!success)
            {
                RCLCPP_ERROR(m_logger, "Failed to delete old Gazebo model: %s", model.model_name.c_str());
                return;
            }

            RCLCPP_INFO(m_logger, "Old Gazebo model deleted: %s", model.model_name.c_str());

            // 3. 删除成功后，再生成新的AMR
            if (!m_process_manager->spawn(robot))
            {
                RCLCPP_ERROR(
                    m_logger,
                    "Failed to spawn replacement robot: "
                    "robot=%s instance=%s",
                    robot.robot_id.c_str(),
                    robot.instance_id.c_str());

                return;
            }

            RCLCPP_INFO(
                m_logger,
                "Replacement robot spawned: "
                "robot=%s instance=%s",
                robot.robot_id.c_str(),
                robot.instance_id.c_str());
        });
}

}