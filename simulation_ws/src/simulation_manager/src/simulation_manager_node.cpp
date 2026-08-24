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
    this->declare_parameter("sync_interval", 5);

    const auto warehouse_id = get_parameter("warehouse_id").as_string();
    const auto websocket_url = get_parameter("websocket_url").as_string();
    auto sync_interval = get_parameter("sync_interval").as_int();
    if (sync_interval <= 0)
    {
        RCLCPP_WARN(get_logger(), "Invalid sync_interval=%ld, use 5 seconds", sync_interval);
        sync_interval = 5;
    }

    RCLCPP_INFO(get_logger(), "warehouse_id=%s", warehouse_id.c_str());
    RCLCPP_INFO(get_logger(), "websocket_url=%s", websocket_url.c_str());

    m_edge_client = std::make_shared<EdgeClient>(websocket_url);
    if (!m_edge_client->connect())
    {
        RCLCPP_ERROR(get_logger(), "Failed to connect to Edge Server");
    }

    m_gazebo_client = std::make_shared<GazeboClient>(shared_from_this());
    m_process_manager = std::make_shared<AmrProcessManager>();
    m_lifecycle_manager = std::make_shared<RobotLifecycleManager>(shared_from_this(), m_gazebo_client, m_process_manager);
    m_sync_manager = std::make_shared<RobotSyncManager>(shared_from_this(), m_gazebo_client, m_lifecycle_manager);

    RCLCPP_INFO(this->get_logger(), "simulation_manager started");
    m_timer = this->create_wall_timer(
            std::chrono::seconds(sync_interval),
            std::bind(&SimulationManagerNode::onTimer, this)
        );
}

void SimulationManagerNode::onTimer()
{
    if (!m_edge_client->isConnected())
    {
        RCLCPP_WARN(get_logger(), "Edge Server disconnected");
        return;
    }

    const auto robots = m_edge_client->getRobots();

    RCLCPP_INFO(get_logger(), "Received %zu robots from Edge", robots.size());
    m_sync_manager->sync(robots);
}


}