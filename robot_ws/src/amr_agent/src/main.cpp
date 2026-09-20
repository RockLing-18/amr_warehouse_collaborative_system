#include "amr_agent_node.h"

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<amr_agent::AmrAgentNode>();
    if(!node->init())
    {
        RCLCPP_ERROR(node->get_logger(),  "amr agent init failed");
        rclcpp::shutdown();
        return -1;
    }

    rclcpp::executors::MultiThreadedExecutor executor;
    executor.add_node(node);
    executor.spin();
    
    rclcpp::shutdown();
    return 0;
}