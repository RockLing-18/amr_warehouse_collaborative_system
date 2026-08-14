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

NavigationResponse NavigationManager::navigateToPose(const NavigationRequest& req)
{
    NavigationResponse resp;
    if(!m_action_client->wait_for_action_server(std::chrono::seconds(5)))
    {
        RCLCPP_ERROR(m_node->get_logger(), "Nav2 Action服务未启动");
        resp.accepted = false;
        resp.message = "nav2 action server not available";
        return resp;
    }

    // 单活动导航：忙碌时直接返回，由调用方（amr_navigation 的 server）决策
    if(m_state == NavigationState::ACCEPTING || m_state == NavigationState::NAVIGATING || m_state == NavigationState::CANCELING)
    {
        resp.accepted = false;
        resp.state = m_state;
        resp.message = "robot busy";
        RCLCPP_WARN(m_node->get_logger(), "robot already navigating");
        return resp;
    }

    RCLCPP_INFO(
    m_node->get_logger(),
    "Send goal frame=%s x=%.2f y=%.2f",
    req.goal.header.frame_id.c_str(),
    req.goal.pose.position.x,
    req.goal.pose.position.y);

    NavigateToPose::Goal goal;
    goal.pose = req.goal;
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

    m_state = NavigationState::ACCEPTING;
    m_action_client->async_send_goal(goal, options);

    resp.accepted = true;
    resp.state = NavigationState::ACCEPTING;
    resp.message = "navigation request accepted";
    return resp;
}

void NavigationManager::goalResponseCallback(GoalHandle::SharedPtr goal_handle)
{
    if(!goal_handle)
    {
        RCLCPP_ERROR(m_node->get_logger(), "导航目标被拒绝");
        m_state = NavigationState::FAILED;
        m_goal_handle.reset();

        if(m_result_callback)
            m_result_callback(NavigationState::FAILED);
        
        return;
    }

    m_state = NavigationState::NAVIGATING;
    m_goal_handle = goal_handle;
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
        NavigationFeedback feedbackStatus;
        feedbackStatus.state = NavigationState::NAVIGATING;
        feedbackStatus.current_pose = pose;
        feedbackStatus.navigation_time = feedback->navigation_time;
        feedbackStatus.estimated_time_remaining = feedback->estimated_time_remaining;
        feedbackStatus.distance_remaining = feedback->distance_remaining;
        feedbackStatus.number_of_recoveries = feedback->number_of_recoveries;

        m_feedback_callback(feedbackStatus);
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

    m_goal_handle.reset();
    m_state = NavigationState::IDLE;

    if(m_result_callback)
        m_result_callback(state);
}

void NavigationManager::cancelNavigation()
{
    if(m_goal_handle)
    {
        m_state = NavigationState::CANCELING;
        m_action_client->async_cancel_goal(m_goal_handle);
        RCLCPP_INFO(m_node->get_logger(), "cancel request sent");
    }
}

// bool NavigationManager::isNavigating() const
// {
//     return m_navigating;
// }

NavigationState NavigationManager::getState() const
{
    return m_state;
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
