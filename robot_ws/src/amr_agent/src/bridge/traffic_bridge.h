#pragma once

#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "amr_interfaces/msg/traffic_request.hpp"
#include "amr_interfaces/msg/traffic_response.hpp"

namespace amr_agent
{

class EdgeCommunication;

class TrafficBridge
{
public:
    using TrafficRequest = amr_interfaces::msg::TrafficRequest;
    using TrafficResponse = amr_interfaces::msg::TrafficResponse;

    TrafficBridge(
        rclcpp::Node::SharedPtr node,
        EdgeCommunication* communication,
        const std::string& robot_id);

    void init();

private:
    void onInternalTrafficRequest(const TrafficRequest::SharedPtr request);
    void onMqttTrafficResponse(const std::string& payload);

private:
    rclcpp::Node::SharedPtr m_node;
    EdgeCommunication* m_communication;
    std::string m_robot_id;

    rclcpp::Subscription<TrafficRequest>::SharedPtr m_traffic_request_sub;
    rclcpp::Publisher<TrafficResponse>::SharedPtr m_traffic_response_pub;
};

}
