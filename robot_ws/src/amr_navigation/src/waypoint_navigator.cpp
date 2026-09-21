#include "amr_navigation/waypoint_navigator.h"


namespace amr_navigation
{
WaypointNavigator::WaypointNavigator(rclcpp::Node::SharedPtr node) : m_node(node), m_logger(node->get_logger())
{
    m_client = rclcpp_action::create_client<FollowWaypoints>(node, "follow_waypoints");
}

bool WaypointNavigator::waitForServer()
{
    if(!m_client->wait_for_action_server(std::chrono::seconds(10)))
    {
        RCLCPP_ERROR(m_logger, "follow_waypoints action server unavailable");
        return false;
    }

    return true;
}

bool WaypointNavigator::followWaypoints(const std::vector<geometry_msgs::msg::PoseStamped>& poses)
{
    if(!waitForServer())
        return false;
    
    auto goal_msg = FollowWaypoints::Goal();
    goal_msg.poses = poses;
    auto send_goal_options = rclcpp_action::Client<FollowWaypoints>::SendGoalOptions();
    send_goal_options.feedback_callback = [this](GoalHandle::SharedPtr, const std::shared_ptr<const FollowWaypoints::Feedback> feedback)
    {
        int index = feedback->current_waypoint;
        RCLCPP_INFO(m_logger, "Current waypoint: %d", index);
        if(m_feedback_callback)
        {
            m_feedback_callback(index);
        }
    };

    auto node = m_node.lock();
    if(!node)
        return false;

    auto future_goal = m_client->async_send_goal(goal_msg, send_goal_options);
    if(rclcpp::spin_until_future_complete(node, future_goal) != rclcpp::FutureReturnCode::SUCCESS)
    {
        RCLCPP_ERROR( m_logger, "send waypoint goal failed");
        return false;
    }

    auto m_goal_handle  = future_goal.get();
    if(!m_goal_handle)
    {
        RCLCPP_ERROR(m_logger, "waypoint rejected");
        return false;
    }

    RCLCPP_INFO(m_logger, "waypoint accepted");

    auto result_future = m_client->async_get_result(m_goal_handle);

    while(rclcpp::ok() && result_future.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready)
    {
        rclcpp::spin_some(node);
    }

    auto result = result_future.get();
    switch(result.code)
    {
        case rclcpp_action::ResultCode::SUCCEEDED:
            RCLCPP_INFO( m_logger, "waypoint navigation success");
            return true;
        case rclcpp_action::ResultCode::CANCELED:
            RCLCPP_WARN(m_logger, "waypoint canceled");
            return false;
        default:
            RCLCPP_ERROR(m_logger, "waypoint failed");
            return false;
    }
}

void WaypointNavigator::setFeedbackCallback(FeedbackCallback callback)
{
    m_feedback_callback = callback;
}

bool WaypointNavigator::cancel()
{
    if(!m_goal_handle)
    {
        RCLCPP_WARN(m_logger, "No active navigation goal");
        return false;
    }

    auto node = m_node.lock();
    if(!node)
        return false;
    
    auto future_cancel = m_client->async_cancel_goal(m_goal_handle);
    if(rclcpp::spin_until_future_complete(node,future_cancel) != rclcpp::FutureReturnCode::SUCCESS)
    {
        RCLCPP_ERROR(m_logger, "Cancel failed");
        return false;
    }

    RCLCPP_INFO(m_logger, "Navigation canceled");
    m_active=false;
    m_goal_handle.reset();
    return true;
}

}