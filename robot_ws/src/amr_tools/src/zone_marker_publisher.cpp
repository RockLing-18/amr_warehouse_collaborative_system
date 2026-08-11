#include "amr_tools/zone_marker_publisher.h"
#include <iostream>
#include "rclcpp/rclcpp.hpp"

ZoneMarkerPublisher::ZoneMarkerPublisher(rclcpp::Node::SharedPtr node) : m_node(node)
{
    // m_pub = m_node->create_publisher<visualization_msgs::msg::MarkerArray>("/zone_marker", 10);

    //auto qos = rclcpp::QoS(rclcpp::KeepLast(1)).durability_volatile().reliable();
    auto qos = rclcpp::QoS(rclcpp::KeepLast(10)).transient_local().reliable();
    m_pub = m_node->create_publisher<visualization_msgs::msg::MarkerArray>
    (
        "/zone_marker",
        qos
    );
}

void ZoneMarkerPublisher::publishZones(const std::vector<Zone>& vZone)
{
    // clear();
    
    visualization_msgs::msg::MarkerArray array;

     // DELETEALL放在同一个array，一次性发布，不要单独发
    visualization_msgs::msg::Marker del_all;
    del_all.header.frame_id = "map";
    del_all.header.stamp = m_node->get_clock()->now();
    del_all.action = visualization_msgs::msg::Marker::DELETEALL;
    array.markers.push_back(del_all);
    
    if(vZone.empty())
    {
        std::cout << "vZone is empty" << std::endl;
        m_pub->publish(array);
        return;
    }

    for(const auto& zone : vZone)
    {
        array.markers.push_back(createLineMarker(zone));
        array.markers.push_back(createTextMarker(zone));
    }

    std::cout << "所有区域发布成功！" << std::endl; 
    m_pub->publish(array);
}

void ZoneMarkerPublisher::publishZone(const Zone &zone)
{
    visualization_msgs::msg::MarkerArray array;
    array.markers.push_back(createLineMarker(zone));
    array.markers.push_back(createTextMarker(zone));
    m_pub->publish(array);
}

void ZoneMarkerPublisher::publishPreview(const Zone& zone)
{
    visualization_msgs::msg::MarkerArray array;
    array.markers.push_back(createLineMarker(zone, true));
    array.markers.push_back(createTextMarker(zone, true));
    m_pub->publish(array);
}

visualization_msgs::msg::Marker ZoneMarkerPublisher::createLineMarker(const Zone& zone, bool preview)
{
    visualization_msgs::msg::Marker marker;
    marker.header.frame_id = zone.frame_id;
    marker.header.stamp = m_node->get_clock()->now();
    marker.ns = preview ? "preview_zone_line" : "zone_line";
    marker.id = preview ? 0 : zone.marker_id;
    marker.type = visualization_msgs::msg::Marker::LINE_STRIP;
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
        marker.color.g = 0.0;
        marker.color.b = 0.0;
        marker.color.a = 1.0;
    }
    

    for(auto& p : zone.polygon)
    {
        geometry_msgs::msg::Point point;
        point.x = p.first;
        point.y = p.second;
        point.z = 0.05;
        marker.points.push_back(point);
    }

    // 闭合多边形
    if(!zone.polygon.empty())
    {
        geometry_msgs::msg::Point first;
        first.x = zone.polygon.front().first;
        first.y = zone.polygon.front().second;
        first.z = 0.05;
        marker.points.push_back(first);
    }

    return marker;
}

visualization_msgs::msg::Marker ZoneMarkerPublisher::createTextMarker(const Zone& zone, bool preview)
{
    visualization_msgs::msg::Marker marker;
    marker.header.frame_id = zone.frame_id;
    marker.header.stamp = m_node->get_clock()->now();
    marker.ns = preview ? "preview_zone_text" : "zone_text";
    marker.id = preview ? 0 : zone.marker_id;
    marker.type = visualization_msgs::msg::Marker::TEXT_VIEW_FACING;
    marker.action = visualization_msgs::msg::Marker::ADD;

    /*
        文字位置
        简单取polygon中心点
    */
    // double x = 0;
    // double y = 0;
    // for(auto& p:zone.polygon)
    // {
    //     x += p.first;
    //     y += p.second;
    // }

    // if(!zone.polygon.empty())
    // {
    //     x /= zone.polygon.size();
    //     y /= zone.polygon.size();
    // }

    auto center = calculateCentroid(zone.polygon);
    double x = center.first;
    double y = center.second;

    marker.pose.position.x = x;
    marker.pose.position.y = y;
    marker.pose.position.z = 0.8;

    marker.pose.orientation.x = 0.0;
    marker.pose.orientation.y = 0.0;
    marker.pose.orientation.z = 0.0;
    marker.pose.orientation.w = 1.0;

    marker.scale.z = 0.8;
    marker.color.r = 0.0;
    marker.color.g = 0.0;
    marker.color.b = 1.0;
    marker.color.a = 1.0;
    marker.text = zone.info.name;
    return marker;
}

void ZoneMarkerPublisher::clearPreview()
{
    visualization_msgs::msg::MarkerArray array;
    visualization_msgs::msg::Marker markerLine;
    markerLine.header.frame_id = "map";
    markerLine.header.stamp = m_node->get_clock()->now();
    markerLine.ns = "preview_zone_line";
    markerLine.id = 0;
    markerLine.action = visualization_msgs::msg::Marker::DELETE;
    array.markers.push_back(markerLine);

    visualization_msgs::msg::Marker markerText = markerLine;
    markerText.ns = "preview_zone_text";
    array.markers.push_back(markerText);
    m_pub->publish(array);
}

void ZoneMarkerPublisher::clear()
{
    visualization_msgs::msg::MarkerArray array;
    visualization_msgs::msg::Marker marker;
    marker.header.frame_id = "map";
    marker.header.stamp = m_node->get_clock()->now();
    marker.action = visualization_msgs::msg::Marker::DELETEALL;
    array.markers.push_back(marker);
    m_pub->publish(array);
}

std::pair<double,double> ZoneMarkerPublisher::calculateCentroid(const std::vector<std::pair<double,double>>& polygon)
{
    double min_x = std::numeric_limits<double>::max();
    double max_x = -min_x;

    double min_y = std::numeric_limits<double>::max();
    double max_y = -min_y;


    for(const auto& p : polygon)
    {
        min_x = std::min(min_x, p.first);
        max_x = std::max(max_x, p.first);

        min_y = std::min(min_y, p.second);
        max_y = std::max(max_y, p.second);
    }


    double x = (min_x + max_x)/2;
    double y = (min_y + max_y)/2;
    return {x, y};


    // double area = 0.0;
    // double cx = 0.0;
    // double cy = 0.0;
    // int n = polygon.size();

    // std::cout << "polygon.size:" << n << std::endl;
    // for(int i=0; i<n; i++)
    // {
    //     auto p1 = polygon[i];
    //     auto p2 = polygon[(i+1) % n];

    //     double cross = p1.first * p2.second - p2.first * p1.second;
    //     area += cross;
    //     cx += (p1.first+p2.first)*cross;
    //     cy += (p1.second+p2.second)*cross;
    // }

    // area *= 0.5;
    // if(std::fabs(area) < 1e-6)
    // {
    //     return polygon.front();
    // }

    // cx /= (6.0*area);
    // cy /= (6.0*area);
    // return {cx,cy};
}