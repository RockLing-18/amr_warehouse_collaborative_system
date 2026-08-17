#include "amr_navigation/tf_helper.h"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"
#include "tf2/LinearMath/Quaternion.h"
#include "tf2/LinearMath/Matrix3x3.h"

namespace amr_navigation
{

TFHelper::TFHelper(rclcpp::Node::SharedPtr node): m_node(node)
{
    m_buffer = std::make_unique<tf2_ros::Buffer>(m_node->get_clock());
    m_listener = std::make_shared<tf2_ros::TransformListener>(*m_buffer);
}

bool TFHelper::getRobotPose(RobotPose &pose)
{
    constexpr char target_frame[] = "map";
    constexpr char source_frame[] = "base_footprint";

    if (!m_buffer->canTransform(
            target_frame,
            source_frame,
            tf2::TimePointZero))
    {
        return false;
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
            m_node->get_logger(),
            *m_node->get_clock(),
            5000,
            "TF lookup failed: %s",
            ex.what());

        return false;
    }
}

}