#include "edge_server/edge_server_node.h"

int main(int argc,char** argv)
{
    rclcpp::init(argc,argv);

    auto node = std::make_shared<edge_server::EdgeServerNode>();
    node->init();
    
    rclcpp::executors::MultiThreadedExecutor executor;
    executor.add_node(node);
    executor.spin();

    rclcpp::shutdown();

    return 0;
}