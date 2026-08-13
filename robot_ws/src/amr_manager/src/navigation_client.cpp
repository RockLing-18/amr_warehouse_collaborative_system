#include "amr_manager/navigation_client.h"

namespace amr_manager
{

NavigationClient::NavigationClient(rclcpp::Node::SharedPtr node)
: node_(node)
{
  // 相对名创建，robot namespace（如 /robot_01）自动生效
  action_client_ = rclcpp_action::create_client<AmrNavigateToPose>(node_, "navigate_to_pose");
  pose_service_client_ = node_->create_client<GetRobotPose>("get_robot_pose");
  pose_sub_ = node_->create_subscription<RobotPose>(
    "robot_pose", 10,
    std::bind(&NavigationClient::poseSubscriptionCallback, this, std::placeholders::_1));
}

bool NavigationClient::waitForActionServer(const std::chrono::seconds & timeout)
{
  return action_client_->wait_for_action_server(timeout);
}

void NavigationClient::navigateToPose(
  uint64_t navigation_id,
  const geometry_msgs::msg::PoseStamped & goal)
{
  auto goal_msg = AmrNavigateToPose::Goal();
  goal_msg.navigation_id = navigation_id;
  goal_msg.goal_pose = goal;

  auto send_goal_options = rclcpp_action::Client<AmrNavigateToPose>::SendGoalOptions();
  send_goal_options.feedback_callback =
    [this](GoalHandle::SharedPtr,
           const std::shared_ptr<const AmrNavigateToPose::Feedback> feedback)
    {
      if (feedback_cb_) { feedback_cb_(feedback); }
    };
  send_goal_options.result_callback =
    [this](const GoalHandle::WrappedResult & result)
    {
      if (result_cb_) { result_cb_(result); }
    };

  RCLCPP_INFO(node_->get_logger(), "send navigate goal, navigation_id=%lu", navigation_id);
  // TODO:
  //   1. 记录 current_goal_handle_（在 send_goal_options.goal_response_callback 中）
  //   2. navigation_id 与当前不一致时先 cancel 旧目标
  action_client_->async_send_goal(goal_msg, send_goal_options);
}

void NavigationClient::cancelNavigation()
{
  // TODO: current_goal_handle_ 为空则忽略，否则 action_client_->async_cancel_goal(...)
  RCLCPP_WARN(node_->get_logger(), "cancelNavigation: TODO implement");
}

NavigationClient::RobotPose::SharedPtr NavigationClient::getLatestPose() const
{
  std::lock_guard<std::mutex> lock(pose_mutex_);
  return latest_pose_;
}

void NavigationClient::queryPoseAsync(PoseQueryCallback callback)
{
  auto request = std::make_shared<GetRobotPose::Request>();
  request->frame_id = "base_footprint";

  // TODO: 服务不可用（!pose_service_client_->service_is_ready()）时的失败回调
  pose_service_client_->async_send_request(
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
  std::lock_guard<std::mutex> lock(pose_mutex_);
  latest_pose_ = msg;
}

void NavigationClient::setFeedbackCallback(FeedbackCallback cb)
{
  feedback_cb_ = std::move(cb);
}

void NavigationClient::setResultCallback(ResultCallback cb)
{
  result_cb_ = std::move(cb);
}

}  // namespace amr_manager
