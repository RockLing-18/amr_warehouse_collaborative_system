#include "edge_server/edge_server_node.h"

namespace edge_server
{
EdgeServerNode::EdgeServerNode(): Node("edge_server")
{
}

void EdgeServerNode::init()
{
    std::string host = this->declare_parameter<std::string>("websocket_host", "0.0.0.0");
    int port = this->declare_parameter<int>("websocket_port", 9000);
    int period = this->declare_parameter<int>("robot_list_period_ms", 1000);
    m_robot_manager = std::make_shared<RobotManager>();
    m_websocket = std::make_shared<WebSocketServer>();
    if(!m_websocket->start(host, port, "robotList-protocol"))
    {
        RCLCPP_ERROR(
            this->get_logger(),
            "websocket start failed");
    }
    else
    {
        RCLCPP_INFO(
            this->get_logger(),
            "WebSocket server started, port=%d",
            port);
    }

    m_robot_publisher = std::make_shared<RobotListPublisher>(m_robot_manager, m_websocket);

    m_robot_publisher->start(period);

    RCLCPP_INFO(this->get_logger(), "edge server start");
}

}


