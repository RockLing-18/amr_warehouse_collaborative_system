#pragma once
#include <memory>
#include "rclcpp/rclcpp.hpp"
#include "visualization_msgs/msg/marker_array.hpp"
#include "data_define.h"

class StationMarkerPublisher
{
public:
    explicit StationMarkerPublisher(rclcpp::Node::SharedPtr node);

    /**
     * 发布station可视化(Rviz2中显示)
     */
    void publishStations(const std::vector<Station>& vStation);
    void publishStation(const Station& station);
    void publishPreview(const Station& station);

    /**
     * 清除所有marker
     */
    void clear();

    // 清楚预览区域
    void clearPreview(int locationTotal);


private:
    visualization_msgs::msg::Marker createPointMarker(const Location& location, bool preview = false);

private:
    rclcpp::Node::SharedPtr m_node;
    rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr m_pub;
};