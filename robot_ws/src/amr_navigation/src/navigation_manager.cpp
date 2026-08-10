#include "amr_navigation/navigation_manager.h"

namespace amr_navigation
{
NavigationManager::NavigationManager(rclcpp::Node::SharedPtr node) : m_node(node)
{
    m_action_client = rclcpp_action::create_client<NavigateToPose>(
            m_node,
            "navigate_to_pose"
        );
}

bool NavigationManager::navigateToPose(const geometry_msgs::msg::PoseStamped &goal_pose)
{
    if(!m_action_client->wait_for_action_server(std::chrono::seconds(5)))
    {
        RCLCPP_ERROR(m_node->get_logger(), "Nav2 Action服务未启动");
        return false;
    }

    RCLCPP_INFO(
    m_node->get_logger(),
    "Send goal frame=%s x=%.2f y=%.2f",
    goal_pose.header.frame_id.c_str(),
    goal_pose.pose.position.x,
    goal_pose.pose.position.y);

    NavigateToPose::Goal goal;
    goal.pose = goal_pose;
    auto options = rclcpp_action::Client<NavigateToPose>::SendGoalOptions();
    options.goal_response_callback = std::bind(&NavigationManager::goalResponseCallback, this, std::placeholders::_1);
    options.feedback_callback = std::bind(
            &NavigationManager::feedbackCallback,
            this,
            std::placeholders::_1,
            std::placeholders::_2
        );

    options.result_callback = std::bind(
            &NavigationManager::resultCallback,
            this,
            std::placeholders::_1
        );

    m_action_client->async_send_goal(goal, options);
    return true;
}

void NavigationManager::goalResponseCallback(GoalHandle::SharedPtr goal_handle)
{
    if(!goal_handle)
    {
        RCLCPP_ERROR(m_node->get_logger(), "导航目标被拒绝");
        return;
    }

    m_goal_handle = goal_handle;
    m_navigating = true;
    RCLCPP_INFO(m_node->get_logger(), "导航目标已接受");
}

void NavigationManager::feedbackCallback(GoalHandle::SharedPtr, const std::shared_ptr<const NavigateToPose::Feedback> feedback)
{
    auto pose = feedback->current_pose;

    RCLCPP_INFO(
        m_node->get_logger(),
        "current(%.2f %.2f), remain %.2f",
        pose.pose.position.x,
        pose.pose.position.y,
        feedback->distance_remaining
    );


    if(m_feedback_callback)
    {
        NavigationStatus status;
        status.state = NavigationState::NAVIGATING;
        status.current_pose = pose;
        status.distance_remaining = feedback->distance_remaining;

        m_feedback_callback(status);
    }
}

void NavigationManager::resultCallback(const GoalHandle::WrappedResult &result)
{
    RCLCPP_INFO(
        m_node->get_logger(),
        "导航结束 result code=%d",
        static_cast<int>(result.code)
    );

    NavigationState state = NavigationState::FAILED;
    m_navigating = false;
    switch(result.code)
    {
        case rclcpp_action::ResultCode::SUCCEEDED:
            RCLCPP_INFO(m_node->get_logger(), "导航成功");
            state = NavigationState::SUCCEEDED;
            break;
        case rclcpp_action::ResultCode::ABORTED:
            RCLCPP_ERROR(m_node->get_logger(), "导航失败");
            state = NavigationState::FAILED;
            break;
        case rclcpp_action::ResultCode::CANCELED:
            RCLCPP_WARN(m_node->get_logger(), "导航取消");
            state = NavigationState::CANCELED;
            break;
        default:
            break;
    }

    if(m_result_callback)
        m_result_callback(state);
}

void NavigationManager::cancelNavigation()
{
    if(m_goal_handle && m_navigating)
    {
        m_action_client->async_cancel_goal(m_goal_handle);
    }
}

bool NavigationManager::isNavigating() const
{
    return m_navigating;
}

void NavigationManager::setFeedbackCallback(FeedbackCallback cb)
{
    m_feedback_callback = cb;
}

void NavigationManager::setResultCallback(ResultCallback cb)
{
    m_result_callback = cb;
}

}