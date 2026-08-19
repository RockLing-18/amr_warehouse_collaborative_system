#include "simulation_manager/simulation_manager_node.h"


namespace simulation_manager
{

SimulationManagerNode::SimulationManagerNode() : Node("simulation_manager")
{
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