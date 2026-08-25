#include "simulation_manager/simulation_manager_node.h"


namespace simulation_manager
{

SimulationManagerNode::SimulationManagerNode() : Node("simulation_manager")
{
}

void SimulationManagerNode::init()
{
    this->declare_parameter("warehouse_id", "");
    this->declare_parameter("websocket_url", "");
    this->declare_parameter("update_gazebo_models_interval", 1);

    const auto warehouse_id = get_parameter("warehouse_id").as_string();
    const auto websocket_url = get_parameter("websocket_url").as_string();
    auto update_gazebo_models_interval = get_parameter("update_gazebo_models_interval").as_int();
    if (update_gazebo_models_interval <= 0)
    {
        RCLCPP_WARN(get_logger(), "Invalid update_gazebo_models_interval=%ld, use 5 seconds", update_gazebo_models_interval);
        update_gazebo_models_interval = 1;
    }

    RCLCPP_INFO(get_logger(), "warehouse_id=%s", warehouse_id.c_str());
    RCLCPP_INFO(get_logger(), "websocket_url=%s", websocket_url.c_str());

    m_gazebo_client = std::make_shared<GazeboClient>(shared_from_this());
    m_process_manager = std::make_shared<AmrProcessManager>();
    m_lifecycle_manager = std::make_shared<RobotLifecycleManager>(shared_from_this(), m_gazebo_client, m_process_manager);
    m_sync_manager = std::make_shared<RobotSyncManager>(shared_from_this(), m_gazebo_client, m_lifecycle_manager);
    m_edge_client = std::make_shared<EdgeClient>(websocket_url);

    RCLCPP_INFO(this->get_logger(), "simulation_manager started");
    m_timer = this->create_wall_timer(
            std::chrono::seconds(update_gazebo_models_interval),
            std::bind(&SimulationManagerNode::onTimerUpdateRobotModels, this)
        );

    // 发起robot 列表订阅
     m_edge_client->setNotifyCallback(
        [this]()
        {
            m_sync_manager->sync(m_edge_client->getRobotList());
        });

    if (!m_edge_client->connect())
    {
        RCLCPP_ERROR(get_logger(), "Failed to connect to Edge Server");
    }
}

void SimulationManagerNode::onTimerUpdateRobotModels()
{
    m_gazebo_client->updateModels();
}


}