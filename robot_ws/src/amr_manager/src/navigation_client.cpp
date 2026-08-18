#include "amr_manager/navigation_client.h"

namespace amr_manager
{

NavigationClient::NavigationClient(rclcpp::Node::SharedPtr node)
: m_node(node)
{
    // 相对名创建，robot namespace（如 /robot_01）
    m_action_client = rclcpp_action::create_client<AmrNavigateToPose>(m_node, "amr_navigate_to_pose");
    m_pose_service_client = m_node->create_client<GetRobotPose>("get_robot_pose");
    m_pose_sub = m_node->create_subscription<RobotPose>(
        "robot_pose", 10,
        std::bind(&NavigationClient::poseSubscriptionCallback, this, std::placeholders::_1));
}

bool NavigationClient::waitForActionServer(const std::chrono::seconds &timeout)
{
    return m_action_client->wait_for_action_server(timeout);
}

void NavigationClient::navigateToPose(const geometry_msgs::msg::PoseStamped &goal)
{
    auto goal_msg = AmrNavigateToPose::Goal();
    goal_msg.goal_pose = goal;

    auto send_goal_options = rclcpp_action::Client<AmrNavigateToPose>::SendGoalOptions();
    send_goal_options.goal_response_callback = [this](const GoalHandle::SharedPtr &goal_handle)
    {
      // 保存句柄：取消 / 状态查询用
      if (goal_handle)
          m_current_goal_handle = goal_handle;
      else
          RCLCPP_WARN(m_node->get_logger(), "navigation goal rejected by server");
    };

    send_goal_options.feedback_callback = [this](GoalHandle::SharedPtr, const std::shared_ptr<const AmrNavigateToPose::Feedback> feedback)
    {
        if (m_feedback_cb) { m_feedback_cb(feedback); }
    };

    send_goal_options.result_callback = [this](const GoalHandle::WrappedResult &result)
    {
        if (m_result_cb) { m_result_cb(result); }
    };

    RCLCPP_INFO(m_node->get_logger(), "send navigate goal, frame=%s x=%.2f y=%.2f",
        goal.header.frame_id.c_str(), goal.pose.position.x, goal.pose.position.y);

    m_action_client->async_send_goal(goal_msg, send_goal_options);
}

void NavigationClient::cancelNavigation()
{
    auto gh = m_current_goal_handle;

    if (!gh)
    {
        RCLCPP_WARN( m_node->get_logger(), "no goal handle" );
        return;
    }

    auto status = gh->get_status();
    if (status == action_msgs::msg::GoalStatus::STATUS_ACCEPTED ||
        status == action_msgs::msg::GoalStatus::STATUS_EXECUTING)
    {
        m_action_client->async_cancel_goal(gh);
        RCLCPP_INFO(m_node->get_logger(), "cancel goal sent");
    }
    else
    {
        RCLCPP_WARN(m_node->get_logger(), "goal is not active, status=%d", status);
    }
}

NavigationClient::RobotPose::SharedPtr NavigationClient::getLatestPose() const
{
    std::lock_guard<std::mutex> lock(m_pose_mutex);
    return m_latest_pose;
}

void NavigationClient::queryPoseAsync(PoseQueryCallback callback)
{
    auto request = std::make_shared<GetRobotPose::Request>();
    request->frame_id = "base_footprint";

    // 服务不可用（!m_pose_service_client->service_is_ready()）时的失败回调
    m_pose_service_client->async_send_request(
        request,
        [callback](rclcpp::Client<GetRobotPose>::SharedFuture future)
        {
            auto response = future.get();
            if (callback)
            {
                callback(response->success, response->x, response->y, response->yaw);
            }
        });
}

void NavigationClient::poseSubscriptionCallback(const RobotPose::SharedPtr msg)
{
    std::lock_guard<std::mutex> lock(m_pose_mutex);
    m_latest_pose = msg;
}

void NavigationClient::setFeedbackCallback(FeedbackCallback cb)
{
    m_feedback_cb = std::move(cb);
}

void NavigationClient::setResultCallback(ResultCallback cb)
{
    m_result_cb = std::move(cb);
}

}
