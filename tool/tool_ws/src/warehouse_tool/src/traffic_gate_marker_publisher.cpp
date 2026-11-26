#include "warehouse_tool/traffic_gate_marker_publisher.h"
#include <iostream>

TrafficGateMarkerPublisher::TrafficGateMarkerPublisher(rclcpp::Node::SharedPtr node)
    : m_node(node)
{
    auto qos = rclcpp::QoS(rclcpp::KeepLast(10)).transient_local().reliable();
    m_pub = m_node->create_publisher<visualization_msgs::msg::MarkerArray>(
        "/traffic_gate_marker", qos);
}

void TrafficGateMarkerPublisher::publishAll(const std::vector<TrafficGate>& gates)
{
    visualization_msgs::msg::MarkerArray array;

    visualization_msgs::msg::Marker del_all;
    del_all.header.frame_id = "map";
    del_all.header.stamp = m_node->get_clock()->now();
    del_all.action = visualization_msgs::msg::Marker::DELETEALL;
    array.markers.push_back(del_all);

    for(const auto& gate : gates)
    {
        array.markers.push_back(createLaneMarker("l1", gate.l1_wait_poses, gate.marker_id, false));
        array.markers.push_back(createLaneMarker("l2", gate.l2_wait_poses, gate.marker_id, false));
    }

    m_pub->publish(array);
}

void TrafficGateMarkerPublisher::publishPreview(const TrafficGate& gate)
{
    visualization_msgs::msg::MarkerArray array;
    array.markers.push_back(createLaneMarker("l1", gate.l1_wait_poses, 1, true));
    array.markers.push_back(createLaneMarker("l2", gate.l2_wait_poses, 2, true));
    m_pub->publish(array);
}

visualization_msgs::msg::Marker TrafficGateMarkerPublisher::createLaneMarker(
    const std::string& lane,
    const std::vector<Pose2D>& poses,
    int marker_id,
    bool preview)
{
    visualization_msgs::msg::Marker marker;
    marker.header.frame_id = "map";
    marker.header.stamp = m_node->get_clock()->now();
    marker.ns = preview ? ("preview_traffic_wait_" + lane) : ("traffic_wait_" + lane);
    marker.id = preview ? (lane == "l1" ? 1 : 2) : marker_id * 2 + (lane == "l1" ? 0 : 1);
    marker.type = visualization_msgs::msg::Marker::SPHERE_LIST;
    marker.action = visualization_msgs::msg::Marker::ADD;
    marker.scale.x = 0.22;
    marker.scale.y = 0.22;
    marker.scale.z = 0.05;

    if(lane == "l1")
    {
        marker.color.r = 0.0f;
        marker.color.g = 1.0f;
        marker.color.b = 0.0f;
    }
    else
    {
        marker.color.r = 1.0f;
        marker.color.g = 0.55f;
        marker.color.b = 0.0f;
    }
    marker.color.a = 1.0f;

    for(const auto& pose : poses)
    {
        geometry_msgs::msg::Point point;
        point.x = pose.x;
        point.y = pose.y;
        point.z = 0.08;
        marker.points.push_back(point);
    }

    return marker;
}

void TrafficGateMarkerPublisher::clearPreview()
{
    visualization_msgs::msg::MarkerArray array;
    for(const char* ns : {"preview_traffic_wait_l1", "preview_traffic_wait_l2"})
    {
        visualization_msgs::msg::Marker marker;
        marker.header.frame_id = "map";
        marker.header.stamp = m_node->get_clock()->now();
        marker.ns = ns;
        marker.id = std::string(ns) == std::string("preview_traffic_wait_l1") ? 1 : 2;
        marker.action = visualization_msgs::msg::Marker::DELETE;
        array.markers.push_back(marker);
    }
    m_pub->publish(array);
}

void TrafficGateMarkerPublisher::clear()
{
    visualization_msgs::msg::MarkerArray array;
    visualization_msgs::msg::Marker marker;
    marker.header.frame_id = "map";
    marker.header.stamp = m_node->get_clock()->now();
    marker.action = visualization_msgs::msg::Marker::DELETEALL;
    array.markers.push_back(marker);
    m_pub->publish(array);
}
