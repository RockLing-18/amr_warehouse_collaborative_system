#include "edge_server/edge_server_node.h"
#include "edge_server/robot/robot_manager.h"
#include "edge_server/robot/robot_list_publisher.h"
#include "edge_server/websocket/websocket_server.h"
#include "edge_server/websocket/websocket_message_router.h"
#include "edge_server/topic/topic_manager.h"

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
    m_webSocketServer = std::make_shared<WebSocketServer>();
    m_topic_manager = std::make_shared<TopicManager>(m_webSocketServer);
    m_ws_router = std::make_shared<WebSocketMessageRouter>(m_topic_manager);

    m_webSocketServer->setMessageCallback(
        [this] (uint64_t clientId, const std::string& msg)
        {
            m_ws_router->onMessage(clientId, msg);
        });

    if(!m_webSocketServer->start(host, port, "robotList-protocol"))
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

    m_robot_publisher = std::make_shared<RobotListPublisher>(m_robot_manager, m_topic_manager);
    m_robot_publisher->start(period);

    RCLCPP_INFO(this->get_logger(), "edge server start");
}

}


