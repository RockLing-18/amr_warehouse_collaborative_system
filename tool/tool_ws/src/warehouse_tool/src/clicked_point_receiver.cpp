#include "warehouse_tool/clicked_point_receiver.h"


ClickedPointReceiver::ClickedPointReceiver(rclcpp::Node::SharedPtr node, PointCallback callback) : m_node(node), m_callback(callback)
{
    m_sub = m_node->create_subscription<geometry_msgs::msg::PointStamped>(
            "/clicked_point",
            10,
            std::bind(&ClickedPointReceiver::callback, this, std::placeholders::_1)
        );
}

void ClickedPointReceiver::start()
{
    m_enable = true;
    RCLCPP_INFO(m_node->get_logger(), "ClickedPointReceiver enable receive");
}

void ClickedPointReceiver::stop()
{
    m_enable = false;
    RCLCPP_INFO(m_node->get_logger(), "ClickedPointReceiver disable receive");
}

void ClickedPointReceiver::callback(const geometry_msgs::msg::PointStamped::SharedPtr msg)
{
    if(!m_enable)
        return;

    if(m_callback)
        m_callback(*msg);
}