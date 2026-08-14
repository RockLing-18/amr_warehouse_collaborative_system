#pragma once
#include <memory>
#include <atomic>
#include <functional>
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "nav2_msgs/action/navigate_to_pose.hpp"
#include "builtin_interfaces/msg/time.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"

namespace amr_navigation
{

enum class NavigationState
{
    IDLE,
    ACCEPTING,
    NAVIGATING,
    CANCELING,
    SUCCEEDED,
    FAILED,
    CANCELED
};

struct NavigationFeedback
{
    NavigationState state;
    geometry_msgs::msg::PoseStamped current_pose;
    builtin_interfaces::msg::Duration navigation_time;
    builtin_interfaces::msg::Duration estimated_time_remaining;
    double distance_remaining;
    uint16_t number_of_recoveries;
};

// 导航请求
struct NavigationRequest
{
    geometry_msgs::msg::PoseStamped goal;
};

// 导航请求响应
struct NavigationResponse
{
    bool accepted{false}; // 请求是否被接受
    NavigationState state{NavigationState::IDLE};
    std::string message;
};

class NavigationManager
{
public:
    using ResultCallback = std::function<void(NavigationState)>;
    using FeedbackCallback = std::function<void(const NavigationFeedback&)>;

    using NavigateToPose = nav2_msgs::action::NavigateToPose;
    using GoalHandle = rclcpp_action::ClientGoalHandle<NavigateToPose>;
    explicit NavigationManager(rclcpp::Node::SharedPtr node);


    /**
     * 发送导航目标
     */
    NavigationResponse navigateToPose(const NavigationRequest& req);

    /**
     * 取消当前导航
     */
    void cancelNavigation();

    /**
     * 是否正在导航
     */
    //bool isNavigating() const;

    NavigationState getState() const;

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
};

}
