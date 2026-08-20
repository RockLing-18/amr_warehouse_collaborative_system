#include "warehouse_tool/station_marker_publisher.h"
#include <iostream>
#include "rclcpp/rclcpp.hpp"

StationMarkerPublisher::StationMarkerPublisher(rclcpp::Node::SharedPtr node) : m_node(node)
{
    // m_pub = m_node->create_publisher<visualization_msgs::msg::MarkerArray>("/station_point_marker", 10);

    //auto qos = rclcpp::QoS(rclcpp::KeepLast(1)).durability_volatile().reliable();
    auto qos = rclcpp::QoS(rclcpp::KeepLast(10)).transient_local().reliable();
    m_pub = m_node->create_publisher<visualization_msgs::msg::MarkerArray>
    (
        "/station_point_marker",
        qos
    );
}

void StationMarkerPublisher::publishStations(const std::vector<Station>& vStation)
{
    visualization_msgs::msg::MarkerArray array;

     // DELETEALL放在同一个array，一次性发布，不要单独发
    // visualization_msgs::msg::Marker del_all;
    // del_all.header.frame_id = "map";
    // del_all.header.stamp = m_node->get_clock()->now();
    // del_all.action = visualization_msgs::msg::Marker::DELETEALL;
    // array.markers.push_back(del_all);
    
    if(vStation.empty())
    {
        std::cout << "vStation is empty" << std::endl;
        return;
    }

    for(const auto& item : vStation)
    {
        for(auto location : item.locations)
            array.markers.push_back(createPointMarker(location));
    }

    std::cout << "所有站点发布成功！" << std::endl; 
    m_pub->publish(array);
}

void StationMarkerPublisher::publishStation(const Station &station)
{
    visualization_msgs::msg::MarkerArray array;
    for(auto location : station.locations)
        array.markers.push_back(createPointMarker(location));
    
    m_pub->publish(array);
}

void StationMarkerPublisher::publishPreview(const Station& station)
{
    visualization_msgs::msg::MarkerArray array;
    int marker_id = 1000;
    for(auto location : station.locations)
    {
        location.marker_id = marker_id++;
        array.markers.push_back(createPointMarker(location, true));
    }
        
    m_pub->publish(array);
}

visualization_msgs::msg::Marker StationMarkerPublisher::createPointMarker(const Location& location, bool preview)
{
    visualization_msgs::msg::Marker marker;
    marker.header.frame_id = "map";
    marker.header.stamp = m_node->get_clock()->now();
    marker.ns = preview ? "preview_station_point" : "station_point";
    marker.id = location.marker_id;
    marker.type = visualization_msgs::msg::Marker::SPHERE;
    marker.action = visualization_msgs::msg::Marker::ADD;

    marker.pose.position.x = location.pose.x;
    marker.pose.position.y = location.pose.y;
    marker.pose.position.z = 0.05;
    marker.pose.orientation.w = 1.0;

    marker.scale.x=0.25;
    marker.scale.y=0.25;
    marker.scale.z=0.05;

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
        marker.color.g = 0.5;
        marker.color.b = 0.0;
        marker.color.a = 1.0;
    }

    return marker;
}

void StationMarkerPublisher::clearPreview(int locationTotal)
{
    visualization_msgs::msg::MarkerArray array;
    for(int i = 0; i < locationTotal; ++i)
    {
        visualization_msgs::msg::Marker marker;
        marker.header.frame_id = "map";
        marker.header.stamp = m_node->get_clock()->now();
        marker.ns = "preview_station_point";
        marker.id = 1000 + i;
        marker.action = visualization_msgs::msg::Marker::DELETE;
        array.markers.push_back(marker);
    }
    
    m_pub->publish(array);
}