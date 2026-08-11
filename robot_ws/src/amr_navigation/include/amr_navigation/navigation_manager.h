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
    ACCEPTING,
    NAVIGATING,
    CANCELING,
    SUCCEEDED,
    FAILED,
    CANCELED
};

struct NavigationFeedback
{
    uint64_t navigation_id;
    NavigationState state;
    double distance_remaining;
    geometry_msgs::msg::PoseStamped current_pose;

    // 暂未使用
    // PoseStamped goal_pose;
    // double distance_remaining;
    // double navigation_time;
};

// 导航请求
struct NavigationRequest
{
    uint64_t navigation_id;  // 导航id
    geometry_msgs::msg::PoseStamped goal;
};

// 导航请求响应
struct NavigationResponse
{
    bool accepted{false}; // 请求是否被接受
    uint64_t navigation_id{0};  // 当前导航id, 与请求下发不一致时，说明当前存在导航，需主动取消(当为0时是其他错误)
    NavigationState state{NavigationState::IDLE};
    std::string message;
};

class NavigationManager
{
public:
    using ResultCallback = std::function<void(uint64_t, NavigationState)>;
    using FeedbackCallback = std::function<void(const NavigationFeedback&)>;

    using NavigateToPose = nav2_msgs::action::NavigateToPose;
    using GoalHandle = rclcpp_action::ClientGoalHandle<NavigateToPose>;
    explicit NavigationManager(rclcpp::Node::SharedPtr node);


    /**
     * 发送导航目标
     */
    NavigationResponse navigateToPose(const NavigationRequest& req);

    /**
     * 取消导航
     */
    void cancelNavigation(uint64_t navigationId);

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
    uint64_t m_navigation_id{0};
    GoalHandle::SharedPtr m_goal_handle;
    FeedbackCallback m_feedback_callback;
    ResultCallback m_result_callback;
    NavigationState m_state{NavigationState::IDLE};
};

}