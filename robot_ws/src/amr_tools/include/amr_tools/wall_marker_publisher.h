#pragma once
#include <memory>
#include "rclcpp/rclcpp.hpp"
#include "visualization_msgs/msg/marker_array.hpp"
#include "data_define.h"

class WallMarkerPublisher
{
public:
    explicit WallMarkerPublisher(rclcpp::Node::SharedPtr node);

    /**
     * 发布wall可视化(Rviz2中显示)
     */
    void publishWalls(const std::vector<Wall>& vWall);
    void publishWall(const Wall& wall);
    void publishPreview(const Wall& wall);

    /**
     * 清除所有marker
     */
    void clear();

    // 清楚预览区域
    void clearPreview();


private:
    visualization_msgs::msg::Marker createLineMarker(const Wall& wall, bool preview = false);

private:
    rclcpp::Node::SharedPtr m_node;
    rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr m_pub;
};