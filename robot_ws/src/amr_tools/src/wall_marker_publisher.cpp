#include "amr_tools/wall_marker_publisher.h"
#include <iostream>
#include "rclcpp/rclcpp.hpp"

WallMarkerPublisher::WallMarkerPublisher(rclcpp::Node::SharedPtr node) : m_node(node)
{
    // m_pub = m_node->create_publisher<visualization_msgs::msg::MarkerArray>("/wall_marker", 10);

    //auto qos = rclcpp::QoS(rclcpp::KeepLast(1)).durability_volatile().reliable();
    auto qos = rclcpp::QoS(rclcpp::KeepLast(10)).transient_local().reliable();
    m_pub = m_node->create_publisher<visualization_msgs::msg::MarkerArray>
    (
        "/wall_marker",
        qos
    );
}

void WallMarkerPublisher::publishWalls(const std::vector<Wall>& vWall)
{
    // clear();
    
    visualization_msgs::msg::MarkerArray array;

     // DELETEALL放在同一个array，一次性发布，不要单独发
    visualization_msgs::msg::Marker del_all;
    del_all.header.frame_id = "map";
    del_all.header.stamp = m_node->get_clock()->now();
    del_all.action = visualization_msgs::msg::Marker::DELETEALL;
    array.markers.push_back(del_all);
    
    if(vWall.empty())
    {
        std::cout << "vWall is empty" << std::endl;
        m_pub->publish(array);
        return;
    }

    for(const auto& wall : vWall)
    {
        array.markers.push_back(createLineMarker(wall));
    }

    std::cout << "所有墙壁发布成功！" << std::endl; 
    m_pub->publish(array);
}

void WallMarkerPublisher::publishWall(const Wall &wall)
{
    visualization_msgs::msg::MarkerArray array;
    array.markers.push_back(createLineMarker(wall));
    m_pub->publish(array);
}

void WallMarkerPublisher::publishPreview(const Wall& wall)
{
    visualization_msgs::msg::MarkerArray array;
    array.markers.push_back(createLineMarker(wall, true));
    m_pub->publish(array);
}

visualization_msgs::msg::Marker WallMarkerPublisher::createLineMarker(const Wall& wall, bool preview)
{
    visualization_msgs::msg::Marker marker;
    marker.header.frame_id = "map";
    marker.header.stamp = m_node->get_clock()->now();
    marker.ns = preview ? "preview_wall_line" : "wall_line";
    marker.id = preview ? 0 : wall.marker_id;
    marker.type = visualization_msgs::msg::Marker::LINE_LIST;
    marker.action = visualization_msgs::msg::Marker::ADD;
    marker.scale.x = 0.05;

    // 颜色
    if(preview)
    {
        marker.color.r = 0.0;
        marker.color.g = 1.0;
        marker.color.b = 0.0;
        marker.color.a = 1.0;   
    }
    else
    {
        marker.color.r = 1.0;
        marker.color.g = 1.0;
        marker.color.b = 0.0;
        marker.color.a = 1.0;
    }
    

    for(auto& p : wall.line)
    {
        geometry_msgs::msg::Point point;
        point.x = p.first;
        point.y = p.second;
        point.z = 0.05;
        marker.points.push_back(point);
    }

    return marker;
}

void WallMarkerPublisher::clearPreview()
{
    visualization_msgs::msg::MarkerArray array;
    visualization_msgs::msg::Marker markerLine;
    markerLine.header.frame_id = "map";
    markerLine.header.stamp = m_node->get_clock()->now();
    markerLine.ns = "preview_wall_line";
    markerLine.id = 0;
    markerLine.action = visualization_msgs::msg::Marker::DELETE;
    array.markers.push_back(markerLine);

    m_pub->publish(array);
}

void WallMarkerPublisher::clear()
{
    visualization_msgs::msg::MarkerArray array;
    visualization_msgs::msg::Marker marker;
    marker.header.frame_id = "map";
    marker.header.stamp = m_node->get_clock()->now();
    marker.action = visualization_msgs::msg::Marker::DELETEALL;
    array.markers.push_back(marker);
    m_pub->publish(array);
}