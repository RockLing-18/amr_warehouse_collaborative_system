#pragma once
#include <memory>
#include <string>
#include <vector>
#include "rclcpp/rclcpp.hpp"
#include "visualization_msgs/msg/marker_array.hpp"
#include "data_define.h"

class TrafficGateMarkerPublisher
{
public:
    explicit TrafficGateMarkerPublisher(rclcpp::Node::SharedPtr node);

    void publishAll(const std::vector<TrafficGate>& gates);
    void publishPreview(const TrafficGate& gate);
    void clearPreview();
    void clear();

private:
    visualization_msgs::msg::Marker createLaneMarker(
        const std::string& lane,
        const std::vector<Pose2D>& poses,
        int marker_id,
        bool preview);

private:
    rclcpp::Node::SharedPtr m_node;
    rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr m_pub;
};
