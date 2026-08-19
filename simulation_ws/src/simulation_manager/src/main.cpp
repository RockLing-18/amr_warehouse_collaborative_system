#include "simulation_manager/simulation_manager_node.h"

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<simulation_manager::SimulationManagerNode >();

    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}