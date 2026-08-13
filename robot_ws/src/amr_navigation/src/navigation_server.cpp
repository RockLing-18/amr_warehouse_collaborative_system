// navigation_server.cpp
// amr_navigation 对外服务节点（代码框架，业务逻辑见 TODO）
//
// 对外接口（全部相对名，robot namespace 实例化）:
//   action : navigate_to_pose (amr_navigation::action::AmrNavigateToPose)
//   service: get_robot_pose   (amr_navigation::srv::GetRobotPose)
//   topic  : robot_pose       (amr_navigation::msg::RobotPose, 默认 1Hz)
//
// 启动（使用 robot namespace）:
//   ros2 run amr_navigation navigation_server --ros-args -r __ns:=/robot_01

#include <chrono>
#include <functional>
#include <memory>
#include <thread>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"

#include "amr_navigation/action/amr_navigate_to_pose.hpp"
#include "amr_interfaces/msg/navigation_state.hpp"
#include "amr_navigation/srv/get_robot_pose.hpp"
#include "amr_navigation/msg/robot_pose.hpp"
#include "amr_navigation/tf_helper.h"
#include "amr_navigation/navigation_manager.h"

namespace amr_navigation
{

class NavigationServer : public rclcpp::Node
{
public:
	using AmrNavigateToPose = amr_navigation::action::AmrNavigateToPose;
	using GoalHandle = rclcpp_action::ServerGoalHandle<AmrNavigateToPose>;
	using GetRobotPose = amr_navigation::srv::GetRobotPose;
	using RobotPoseMsg = amr_navigation::msg::RobotPose;
	using NavigationState = amr_interfaces::msg::NavigationState;
	
	NavigationServer() : Node("navigation_server")
	{
		this->declare_parameter("pose_publish_rate_hz", 1.0);
		this->declare_parameter("use_sim_time", true);
	}
	
	void init()
	{
		m_tf_helper = std::make_shared<TFHelper>(shared_from_this());
		m_navigation_manager = std::make_shared<amr_navigation::NavigationManager>(shared_from_this());
	
		// 位姿话题（相对名 -> /robot_01/robot_pose）
		m_robot_pose_pub = this->create_publisher<RobotPoseMsg>("robot_pose", 10);
	
		// 位姿查询服务（相对名 -> /robot_01/get_robot_pose）
		m_get_robot_pose_srv = this->create_service<GetRobotPose>(
			"get_robot_pose",
			std::bind(&NavigationServer::handleGetRobotPose, this,
				std::placeholders::_1, std::placeholders::_2));
	
		// 导航 Action server（相对名 -> /robot_01/navigate_to_pose）
		m_nav_action_server = rclcpp_action::create_server<AmrNavigateToPose>(
			this,
			"navigate_to_pose",
			std::bind(&NavigationServer::handleGoal, this,
				std::placeholders::_1, std::placeholders::_2),
			std::bind(&NavigationServer::handleCancel, this, std::placeholders::_1),
			std::bind(&NavigationServer::handleAccepted, this, std::placeholders::_1));
	
		// 1Hz 位姿发布
		double rate = this->get_parameter("pose_publish_rate_hz").as_double();
		m_pose_timer = this->create_wall_timer(
			std::chrono::duration<double>(1.0 / rate),
			std::bind(&NavigationServer::publishPose, this));
	
		RCLCPP_INFO(this->get_logger(), "navigation_server init done");
	}

private:
	// ---------------- robot_pose 发布 ----------------
	void publishPose()
	{
		RobotPoseMsg msg;
		msg.pose.header.frame_id = "map";
		msg.pose.header.stamp = this->now();
	
		amr_navigation::RobotPose tf_pose{};
		msg.pose_valid = m_tf_helper->getRobotPose(tf_pose);
		if (msg.pose_valid)
		{
			msg.pose.pose.position.x = tf_pose.x;
			msg.pose.pose.position.y = tf_pose.y;
			// TODO: yaw -> 四元数，填充 msg.pose.pose.orientation
			msg.pose.pose.orientation.w = 1.0;
		}
		m_robot_pose_pub->publish(msg);
	}
	
	// ---------------- get_robot_pose 服务 ----------------
	void handleGetRobotPose(
		const std::shared_ptr<GetRobotPose::Request> request,
		std::shared_ptr<GetRobotPose::Response> response)
	{
		(void)request;
		// TODO:
		//   1. 查询 TF: map -> request->frame_id（默认 base_footprint）
		//   2. 填充 response->x / y / yaw / stamp / success / message
		response->success = false;
		response->message = "TODO: implement";
	}
	
	// ---------------- AmrNavigateToPose action ----------------
	rclcpp_action::GoalResponse handleGoal(
		const rclcpp_action::GoalUUID & uuid,
		const std::shared_ptr<const AmrNavigateToPose::Goal> goal)
	{
		(void)uuid;
		RCLCPP_INFO(this->get_logger(), "receive goal, navigation_id=%lu",
			goal->navigation_id);
		// TODO:
		//   1. 校验 goal_pose（frame_id / 坐标合法性）
		//   2. navigation_id 与当前导航不一致时，先取消旧导航（NavigationManager 语义）
		return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
	}
	
	rclcpp_action::CancelResponse handleCancel(
		const std::shared_ptr<GoalHandle> goal_handle)
	{
		(void)goal_handle;
		RCLCPP_INFO(this->get_logger(), "receive cancel request");
		// TODO: 调用 m_navigation_manager->cancelNavigation(current_navigation_id)
		return rclcpp_action::CancelResponse::ACCEPT;
	}
	
	void handleAccepted(const std::shared_ptr<GoalHandle> goal_handle)
	{
		// 在独立线程执行，避免阻塞 action server 回调
		std::thread{std::bind(&NavigationServer::execute, this, goal_handle)}.detach();
	}
	
	void execute(const std::shared_ptr<GoalHandle> goal_handle)
	{
		const auto goal = goal_handle->get_goal();
		auto feedback = std::make_shared<AmrNavigateToPose::Feedback>();
		auto result = std::make_shared<AmrNavigateToPose::Result>();
	
		feedback->navigation_id = goal->navigation_id;
		feedback->state.state = NavigationState::STATE_NAVIGATING;
		goal_handle->publish_feedback(feedback);
	
		// TODO:
		//   1. 用 goal_pose 构造 NavigationRequest 并调用 m_navigation_manager->navigateToPose(req)
		//   2. 通过 NavigationManager 的 feedback/result 回调推进本函数状态
		//   3. 周期发布 feedback（state / distance_remaining / current_pose）
		while (rclcpp::ok() && !goal_handle->is_canceling())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			break;  // TODO: 等待真实导航结果后退出
		}
	
		if (goal_handle->is_canceling())
		{
			result->navigation_id = goal->navigation_id;
			result->success = false;
			result->message = "canceled";
			goal_handle->canceled(result);
			return;
		}
	
		result->navigation_id = goal->navigation_id;
		result->success = true;  // TODO: 按 NavigationManager 结果设置
		result->message = "navigation finished";
		goal_handle->succeed(result);
	}
	
	std::shared_ptr<TFHelper> m_tf_helper;
	std::shared_ptr<amr_navigation::NavigationManager> m_navigation_manager;
	rclcpp::Publisher<RobotPoseMsg>::SharedPtr m_robot_pose_pub;
	rclcpp::Service<GetRobotPose>::SharedPtr m_get_robot_pose_srv;
	rclcpp_action::Server<AmrNavigateToPose>::SharedPtr m_nav_action_server;
	rclcpp::TimerBase::SharedPtr m_pose_timer;
};
}

int main(int argc, char ** argv)
{
	rclcpp::init(argc, argv);
	auto node = std::make_shared<amr_navigation::NavigationServer>();
	node->init();
	rclcpp::spin(node);
	rclcpp::shutdown();
	return 0;
}
