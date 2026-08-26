#pragma once

#include "edge_server/robot/robot_manager.h"
#include "edge_server/robot/robot_list_publisher.h"
#include "edge_server/websocket/websocket_server.h"
#include <rclcpp/rclcpp.hpp>
#include <memory>

namespace edge_server
{

class EdgeServerNode : public rclcpp::Node
{
public:
    EdgeServerNode();

    void init();
private:
    std::shared_ptr<RobotManager> m_robot_manager;
    std::shared_ptr<WebSocketServer> m_websocket;
    std::shared_ptr<RobotListPublisher> m_robot_publisher;
    rclcpp::TimerBase::SharedPtr m_timer;
};

}