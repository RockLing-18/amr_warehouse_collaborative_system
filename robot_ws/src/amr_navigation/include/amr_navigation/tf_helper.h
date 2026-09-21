#pragma once
#include <memory>
#include <string>
#include "rclcpp/rclcpp.hpp"
#include "tf2_ros/buffer.h"
#include "tf2_ros/transform_listener.h"

namespace amr_navigation
{
// 机器人位姿
struct RobotPose
{
    double x;
    double y;
    double yaw;
};

class TFHelper
{

public:
    TFHelper(rclcpp::Node::SharedPtr node);

    /**
     * 获取机器人当前位姿
     * map -> base_footprint
     */
    bool getRobotPose(RobotPose &pose);

private:
    rclcpp::Node::WeakPtr m_node;
    rclcpp::Logger m_logger;
    std::unique_ptr<tf2_ros::Buffer> m_buffer;
    std::shared_ptr<tf2_ros::TransformListener> m_listener;
};


}