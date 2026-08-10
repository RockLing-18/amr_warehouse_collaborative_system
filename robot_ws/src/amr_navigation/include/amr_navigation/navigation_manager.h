#pragma once
#include <memory>
#include <atomic>
#include <functional>
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "nav2_msgs/action/navigate_to_pose.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"

namespace amr_navigation
{

enum class NavigationState
{
    IDLE,
    NAVIGATING,
    SUCCEEDED,
    FAILED,
    CANCELED
};

struct NavigationStatus
{
    NavigationState state;
    double distance_remaining;
    geometry_msgs::msg::PoseStamped current_pose;
};

class NavigationManager
{
public:
    using ResultCallback = std::function<void(NavigationState)>;
    using FeedbackCallback = std::function<void(const NavigationStatus&)>;

    using NavigateToPose = nav2_msgs::action::NavigateToPose;
    using GoalHandle = rclcpp_action::ClientGoalHandle<NavigateToPose>;
    explicit NavigationManager(rclcpp::Node::SharedPtr node);


    /**
     * 发送导航目标
     */
    bool navigateToPose(const geometry_msgs::msg::PoseStamped &goal);

    /**
     * 取消当前导航
     */
    void cancelNavigation();

    /**
     * 是否正在导航
     */
    bool isNavigating() const;

    void setFeedbackCallback(FeedbackCallback cb);
    void setResultCallback(ResultCallback cb);

private:
    void goalResponseCallback(GoalHandle::SharedPtr goal_handle);
    void feedbackCallback(GoalHandle::SharedPtr, const std::shared_ptr<const NavigateToPose::Feedback> feedback);
    void resultCallback(const GoalHandle::WrappedResult &result);

private:
    rclcpp::Node::SharedPtr m_node;
    rclcpp_action::Client<NavigateToPose>::SharedPtr m_action_client;
    GoalHandle::SharedPtr m_goal_handle;
    FeedbackCallback m_feedback_callback;
    ResultCallback m_result_callback;
    NavigationState m_state{NavigationState::IDLE};
    std::atomic<bool> m_navigating{false};
};

}