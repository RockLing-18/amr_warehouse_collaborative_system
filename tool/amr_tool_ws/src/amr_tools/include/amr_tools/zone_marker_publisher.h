#pragma once
#include <memory>
#include "rclcpp/rclcpp.hpp"
#include "visualization_msgs/msg/marker_array.hpp"
#include "data_define.h"

class ZoneMarkerPublisher
{
public:
    explicit ZoneMarkerPublisher(rclcpp::Node::SharedPtr node);

    /**
     * 发布zone可视化(Rviz2中显示)
     */
    void publishZones(const std::vector<Zone>& vZone);
    void publishZone(const Zone& zone);
    void publishPreview(const Zone& zone);

    /**
     * 清除所有marker
     */
    void clear();

    // 清楚预览区域
    void clearPreview();


private:
    visualization_msgs::msg::Marker createLineMarker(const Zone& zone, bool preview = false);
    visualization_msgs::msg::Marker createTextMarker(const Zone& zone, bool preview = false);
    std::pair<double,double> calculateCentroid(const std::vector<std::pair<double,double>>& polygon);

private:
    rclcpp::Node::SharedPtr m_node;
    rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr m_pub;
};