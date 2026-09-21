#pragma once
#include <rclcpp/rclcpp.hpp>
#include <nav2_msgs/action/follow_waypoints.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>

namespace amr_navigation
{
class WaypointNavigator
{
public:
    using FollowWaypoints = nav2_msgs::action::FollowWaypoints;
    using GoalHandle = rclcpp_action::ClientGoalHandle<FollowWaypoints>;
    using FeedbackCallback = std::function<void(int)>;

public:
    explicit WaypointNavigator(rclcpp::Node::SharedPtr node);
    bool waitForServer();
    bool followWaypoints(const std::vector<geometry_msgs::msg::PoseStamped>& poses);
    void setFeedbackCallback(FeedbackCallback callback);
    bool cancel();
private:
    rclcpp::Node::WeakPtr m_node;
    rclcpp::Logger m_logger;
    rclcpp_action::Client<FollowWaypoints>::SharedPtr m_client;
    GoalHandle::SharedPtr m_goal_handle;// 当前导航任务句柄
    FeedbackCallback m_feedback_callback;
    bool m_active{false};
};

}
