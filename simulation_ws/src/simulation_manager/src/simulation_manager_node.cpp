#include "simulation_manager/simulation_manager_node.h"


namespace simulation_manager
{

SimulationManagerNode::SimulationManagerNode() : Node("simulation_manager")
{
    this->declare_parameter("warehouse_id", "");
    this->declare_parameter("websocket_url", "");
    this->declare_parameter("sync_interval", 5);

    RCLCPP_INFO( this->get_logger(), "simulation_manager started");
    m_timer = this->create_wall_timer(
            std::chrono::seconds(5),
            std::bind(&SimulationManagerNode::onTimer, this)
        );
}


void SimulationManagerNode::onTimer()
{
    RCLCPP_INFO( this->get_logger(),"sync simulation state...");
}


}