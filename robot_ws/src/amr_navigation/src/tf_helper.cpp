#include "amr_navigation/tf_helper.h"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"
#include "tf2/LinearMath/Quaternion.h"
#include "tf2/LinearMath/Matrix3x3.h"
#include "tf2_ros/qos.hpp"

namespace amr_navigation
{

TFHelper::TFHelper(rclcpp::Node::SharedPtr node): m_node(node), m_logger(node->get_logger())
{
    m_buffer = std::make_unique<tf2_ros::Buffer>(node->get_clock());

    // 使用相对名订阅，配合命名空间 TF（/robot01/tf、/robot01/tf_static）
    m_tf_sub = node->create_subscription<tf2_msgs::msg::TFMessage>(
        "tf",
        tf2_ros::DynamicListenerQoS(),
        [this](const tf2_msgs::msg::TFMessage::SharedPtr msg)
        {
            for(const auto& transform : msg->transforms)
                m_buffer->setTransform(transform, "tf");
        });
    m_tf_static_sub = node->create_subscription<tf2_msgs::msg::TFMessage>(
        "tf_static",
        tf2_ros::StaticListenerQoS(),
        [this](const tf2_msgs::msg::TFMessage::SharedPtr msg)
        {
            for(const auto& transform : msg->transforms)
                m_buffer->setTransform(transform, "tf_static", true);
        });
}

bool TFHelper::getRobotPose(RobotPose &pose)
{
    constexpr char target_frame[] = "map";
    std::string source_frame = "base_footprint";

    auto node = m_node.lock();
    if(!node)
        return false;

    std::string ns = node->get_namespace();
    if(!ns.empty() && ns != "/")
    {
        if(ns.front() == '/')
            ns.erase(ns.begin());
        source_frame = ns + "_" + source_frame;
    }

    try
    {
        const auto transform = m_buffer->lookupTransform(
            target_frame,
            source_frame,
            tf2::TimePointZero);

        pose.x = transform.transform.translation.x;
        pose.y = transform.transform.translation.y;

        tf2::Quaternion quat;
        tf2::fromMsg(transform.transform.rotation, quat);

        double roll;
        double pitch;

        tf2::Matrix3x3(quat).getRPY(
            roll,
            pitch,
            pose.yaw);

        return true;
    }
    catch (const tf2::TransformException &ex)
    {
        RCLCPP_WARN_THROTTLE(
            m_logger,
            *node->get_clock(),
            5000,
            "TF lookup failed: %s",
            ex.what());

        return false;
    }
}

}
