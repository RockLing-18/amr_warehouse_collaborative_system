#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"

#include "amr_interfaces/action/amr_navigate_to_pose.hpp"
#include "amr_interfaces/srv/get_robot_pose.hpp"
#include "amr_interfaces/msg/robot_pose.hpp"

namespace amr_manager
{

// amr_navigation 客户端统一封装（代码框架，业务逻辑见 TODO）
// 所有接口使用相对名，robot namespace（如 /robot_01）自动生效
class NavigationClient
{
public:
  using AmrNavigateToPose = amr_interfaces::action::AmrNavigateToPose;
  using GoalHandle = rclcpp_action::ClientGoalHandle<AmrNavigateToPose>;
  using RobotPose = amr_interfaces::msg::RobotPose;
  using GetRobotPose = amr_interfaces::srv::GetRobotPose;

  using FeedbackCallback =
    std::function<void(const std::shared_ptr<const AmrNavigateToPose::Feedback> feedback)>;
  using ResultCallback =
    std::function<void(const GoalHandle::WrappedResult & result)>;
  using PoseQueryCallback =
    std::function<void(bool success, double x, double y, double yaw)>;

  explicit NavigationClient(rclcpp::Node::SharedPtr node);

  // 等待 navigation server 可用
  bool waitForActionServer(
    const std::chrono::seconds & timeout = std::chrono::seconds(5));

  // 发送单点导航目标（按坐标导航，纯能力接口；业务身份由调用方自行维护）
  void navigateToPose(const geometry_msgs::msg::PoseStamped & goal);

  // 取消当前导航
  void cancelNavigation();

  // 最近一次 robot_pose（1Hz 订阅缓存；无导航任务时也可用）
  RobotPose::SharedPtr getLatestPose() const;

  // 按需查询位姿（异步服务调用）
  void queryPoseAsync(PoseQueryCallback callback);

  void setFeedbackCallback(FeedbackCallback cb);
  void setResultCallback(ResultCallback cb);

private:
  void poseSubscriptionCallback(const RobotPose::SharedPtr msg);

  rclcpp::Node::SharedPtr node_;
  rclcpp_action::Client<AmrNavigateToPose>::SharedPtr action_client_;
  rclcpp::Client<GetRobotPose>::SharedPtr pose_service_client_;
  rclcpp::Subscription<RobotPose>::SharedPtr pose_sub_;

  // 当前目标句柄：取消 / 状态查询用（每次发送后更新）
  GoalHandle::SharedPtr current_goal_handle_;

  mutable std::mutex pose_mutex_;
  RobotPose::SharedPtr latest_pose_;

  FeedbackCallback feedback_cb_;
  ResultCallback result_cb_;
};

}  // namespace amr_manager
