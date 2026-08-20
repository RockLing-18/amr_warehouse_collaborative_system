#pragma once
#include <memory>
#include <functional>
#include <atomic>
#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/point_stamped.hpp"

class ClickedPointReceiver
{
public:
    using PointCallback =std::function<void(const geometry_msgs::msg::PointStamped&)>;
    ClickedPointReceiver(rclcpp::Node::SharedPtr node, PointCallback callback);
    void start();
    void stop();
private:
    void callback(const geometry_msgs::msg::PointStamped::SharedPtr msg);

private:
    rclcpp::Node::SharedPtr m_node;
    rclcpp::Subscription<geometry_msgs::msg::PointStamped>::SharedPtr m_sub;
    PointCallback m_callback;
    std::atomic<bool> m_enable{false}; // 新增开关
};