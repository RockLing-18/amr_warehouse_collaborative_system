// navigation_server.cpp
// amr_navigation 对外服务节点
//
// 对外接口（全部相对名，robot namespace 实例化）:
//   action : amr_navigate_to_pose (amr_interfaces::action::AmrNavigateToPose)
//   service: get_robot_pose   (amr_interfaces::srv::GetRobotPose)
//   topic  : robot_pose       (amr_interfaces::msg::RobotPose, 默认 1Hz)
//
// 启动（使用 robot namespace）:
//   ros2 run amr_navigation navigation_server --ros-args -r __ns:=/robot_namespace

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "tf2/LinearMath/Quaternion.h"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"

#include "amr_interfaces/action/amr_navigate_to_pose.hpp"
#include "amr_interfaces/srv/get_robot_pose.hpp"
#include "amr_interfaces/msg/navigation_state.hpp"
#include "amr_interfaces/msg/robot_pose.hpp"
#include "amr_interfaces/msg/map_info.hpp"
#include "amr_interfaces/msg/initial_pose.hpp"
#include "nav2_msgs/srv/manage_lifecycle_nodes.hpp"
#include "geometry_msgs/msg/pose_with_covariance_stamped.hpp"
#include "amr_navigation/tf_helper.h"
#include "amr_navigation/navigation_manager.h"
#include "amr_common/ros_interface_names.h"

namespace amr_navigation
{

class NavigationServer : public rclcpp::Node
{
public:
	enum class Nav2State
	{
		STOPPED,
		STARTING_PROCESS,
		STARTING_LIFECYCLE,
		WAITING_FOR_LOCALIZATION,
		READY,
		STOPPING,
		FAILED
	};

	using AmrNavigateToPose = amr_interfaces::action::AmrNavigateToPose;
	using GoalHandle = rclcpp_action::ServerGoalHandle<AmrNavigateToPose>;
	using GetRobotPose = amr_interfaces::srv::GetRobotPose;
	using RobotPoseMsg = amr_interfaces::msg::RobotPose;
	using NavigationState = amr_interfaces::msg::NavigationState;
	using ManageLifecycleNodes = nav2_msgs::srv::ManageLifecycleNodes;
	using InitialPose = amr_interfaces::msg::InitialPose;
	using PoseWithCovarianceStamped = geometry_msgs::msg::PoseWithCovarianceStamped;
	
	NavigationServer() : Node("navigation_server")
	{
		this->declare_parameter("pose_publish_rate_hz", 1.0);
		this->declare_parameter<std::string>(
			"nav2_params_file",
			"/home/zyr/code/amr_warehouse_collaborative_system/robot_ws/src/amr_navigation/config/nav2_params.yaml");
	}

	~NavigationServer()
	{
		stopNav2Process();
	}
	
	void init()
	{
		m_tf_helper = std::make_shared<TFHelper>(shared_from_this());
		m_navigation_manager = std::make_shared<amr_navigation::NavigationManager>(shared_from_this());

		// 注册 NavigationManager 的 feedback/result 回调
		m_navigation_manager->setFeedbackCallback(std::bind(&NavigationServer::onNavigationFeedback, this, std::placeholders::_1));
		m_navigation_manager->setResultCallback(std::bind(&NavigationServer::onNavigationResult, this, std::placeholders::_1));
	
		// 位姿话题（相对名：/robot_namespace/robot_pose）
		m_robot_pose_pub = this->create_publisher<RobotPoseMsg>(
            amr_common::topic::ROBOT_POSE, 10);
	
		// 位姿查询服务（相对名：/robot_namespace/get_robot_pose）
		m_get_robot_pose_srv = this->create_service<GetRobotPose>(
			amr_common::service::GET_ROBOT_POSE,
			std::bind(&NavigationServer::handleGetRobotPose, this, std::placeholders::_1, std::placeholders::_2)
			);
		
		// 导航 Action server（相对名：/robot_namespace/amr_navigate_to_pose
		m_nav_action_server = rclcpp_action::create_server<AmrNavigateToPose>(
			this,
			amr_common::action::AMR_NAVIGATE_TO_POSE,
			std::bind(&NavigationServer::handleGoal, this, std::placeholders::_1, std::placeholders::_2),
			std::bind(&NavigationServer::handleCancel, this, std::placeholders::_1),
			std::bind(&NavigationServer::handleAccepted, this, std::placeholders::_1)
			);
	
		// 1Hz 位姿发布
		double rate = this->get_parameter("pose_publish_rate_hz").as_double();
		m_pose_timer = this->create_wall_timer(
			std::chrono::duration<double>(1.0 / rate),
			std::bind(&NavigationServer::publishPose, this));

		auto map_info_qos = rclcpp::QoS(rclcpp::KeepLast(1));
		map_info_qos.reliable();
		map_info_qos.transient_local();
		m_map_info_sub = this->create_subscription<amr_interfaces::msg::MapInfo>(
			amr_common::topic::MAP_INFO,
			map_info_qos,
			[this](const amr_interfaces::msg::MapInfo::SharedPtr msg)
			{
				handleMapInfo(msg);
			});

		m_lifecycle_nav_client = this->create_client<ManageLifecycleNodes>(
			"lifecycle_manager_navigation/manage_nodes");
		m_lifecycle_loc_client = this->create_client<ManageLifecycleNodes>(
			"lifecycle_manager_localization/manage_nodes");

		m_initial_pose_pub = this->create_publisher<PoseWithCovarianceStamped>(
			"initialpose", 10);
		m_initial_pose_sub = this->create_subscription<InitialPose>(
			amr_common::topic::INITIAL_POSE,
			10,
			[this](const InitialPose::SharedPtr pose)
			{
				m_initial_pose = pose;
				publishInitialPoseIfReady();
			});

		m_lifecycle_retry_timer = this->create_wall_timer(
			std::chrono::seconds(5),
			[this]()
			{
				handleStateMachineTick();
			});
	
		RCLCPP_INFO(this->get_logger(), "navigation_server init done");
	}

	void shutdownNav2Process()
	{
		stopNav2Process();
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
			// yaw -> 四元数，填充 msg.pose.pose.orientation
			tf2::Quaternion q;
        	q.setRPY(0.0, 0.0, tf_pose.yaw);
			msg.pose.pose.orientation = tf2::toMsg(q);
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

		amr_navigation::RobotPose tf_pose{};
		response->success = m_tf_helper->getRobotPose(tf_pose);
		if (response->success)
		{
			response->x = tf_pose.x;
			response->y = tf_pose.y;
			response->yaw = tf_pose.yaw;
			response->stamp = this->now();
			response->message = "ok";
		}
		else
			response->message = "getRobotPose failed";
	}
	
	// ---------------- AmrNavigateToPose action ----------------
	rclcpp_action::GoalResponse handleGoal(
		const rclcpp_action::GoalUUID & uuid,
		const std::shared_ptr<const AmrNavigateToPose::Goal> goal)
	{
		(void)uuid;
		if(!m_map_ready)
		{
			RCLCPP_WARN(get_logger(), "reject navigation goal because map is not ready");
			return rclcpp_action::GoalResponse::REJECT;
		}
		RCLCPP_INFO(this->get_logger(), "receive goal, frame=%s x=%.2f y=%.2f",
			goal->goal_pose.header.frame_id.c_str(),
			goal->goal_pose.pose.position.x,
			goal->goal_pose.pose.position.y);

		//   1. 校验 goal_pose（frame_id / 坐标合法性）
		//   2. 导航冲突（busy）在 execute 中取消旧导航后重试（supersede）
		return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
	}
	
	rclcpp_action::CancelResponse handleCancel(const std::shared_ptr<GoalHandle> goal_handle)
	{
		(void)goal_handle;
		RCLCPP_INFO(this->get_logger(), "receive cancel request");
		// 单 client 单活动导航：取消当前 Nav2 导航即可；
		// 客户端取消的是旧目标时，NavigationManager 无活动句柄则自动 no-op
		m_navigation_manager->cancelNavigation();
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
		auto result = std::make_shared<AmrNavigateToPose::Result>();

		// 客户端可能在执行前就取消了该目标
		if (goal_handle->is_canceling())
		{
			result->success = false;
			result->message = "canceled before start";
			goal_handle->canceled(result);
			return;
		}

		amr_navigation::NavigationRequest req;
		req.goal = goal->goal_pose;

		// 机器人正忙于其他导航：直接取消旧导航（supersede），等待结束后重试本次目标
		amr_navigation::NavigationResponse resp = m_navigation_manager->navigateToPose(req);
		if (!resp.accepted && isBusyState(resp.state))
		{
			RCLCPP_WARN(this->get_logger(), "robot busy, cancel current navigation and retry");
			m_navigation_manager->cancelNavigation();
			amr_navigation::NavigationExecutionState old_state;
			if (waitForNavigationResult(old_state))
				RCLCPP_INFO(this->get_logger(), "old navigation finished, state=%d",
					static_cast<int>(old_state));
			resp = m_navigation_manager->navigateToPose(req);
		}

		if (!resp.accepted)
		{
			result->success = false;
			result->message = resp.message.empty() ? "navigate request rejected" : resp.message;
			goal_handle->abort(result);
			return;
		}

		// 重置结果标记，登记当前活动目标（供 feedback 回调发布）
		{
			std::lock_guard<std::mutex> lock(m_navigation_mutex);
			m_navigation_result_ready = false;
			m_active_goal_handle = goal_handle;
		}

		// 发送初始 NAVIGATING feedback
		auto feedback = std::make_shared<AmrNavigateToPose::Feedback>();
		feedback->state.navigation_state = NavigationState::NAVIGATING;
		goal_handle->publish_feedback(feedback);

		// 阻塞等待 NavigationManager 结果回调
		amr_navigation::NavigationExecutionState final_state;
		if (!waitForNavigationResult(final_state))
		{
			result->success = false;
			result->message = "navigation result timeout";
			goal_handle->abort(result);
			return;
		}

		switch (final_state)
		{
			case amr_navigation::NavigationExecutionState::SUCCEEDED:
				result->success = true;
				result->message = "navigation succeeded";
				goal_handle->succeed(result);
				break;
			case amr_navigation::NavigationExecutionState::CANCELED:
				result->success = false;
				result->message = "canceled";
				goal_handle->canceled(result);
				break;
			default:
				result->success = false;
				result->message = "navigation failed";
				goal_handle->abort(result);
				break;
		}
	}

	bool isBusyState(amr_navigation::NavigationExecutionState state) const
	{
		return state == amr_navigation::NavigationExecutionState::ACCEPTING ||
			state == amr_navigation::NavigationExecutionState::NAVIGATING ||
			state == amr_navigation::NavigationExecutionState::CANCELING;
	}
	
	// ---------------- NavigationManager 回调 ----------------
	void onNavigationFeedback(const amr_navigation::NavigationFeedback &fb)
	{
		std::shared_ptr<GoalHandle> gh;
		{
			std::lock_guard<std::mutex> lock(m_navigation_mutex);
			gh = m_active_goal_handle;
		}
		if (!gh)
			return;

		auto feedback = std::make_shared<AmrNavigateToPose::Feedback>();
		// 枚举顺序与 NavigationState 消息常量一致（IDLE=0 ... CANCELED=6）
		feedback->state.navigation_state = static_cast<uint8_t>(fb.state);
		feedback->current_pose = fb.current_pose;
		feedback->navigation_time = fb.navigation_time;
		feedback->estimated_time_remaining = fb.estimated_time_remaining;
		feedback->distance_remaining = static_cast<float>(fb.distance_remaining);
		feedback->number_of_recoveries = fb.number_of_recoveries;
		gh->publish_feedback(feedback);
	}
	
	void onNavigationResult(amr_navigation::NavigationExecutionState state)
	{
		{
			std::lock_guard<std::mutex> lock(m_navigation_mutex);
			m_navigation_result = state;
			m_navigation_result_ready = true;
			m_active_goal_handle.reset();
		}
		m_navigation_cv.notify_all();
	}
	
	bool waitForNavigationResult(amr_navigation::NavigationExecutionState & out_state)
	{
		// 单活动导航：同一时刻只有一个 execute 在等待，ready 标志即可匹配
		// 超时时间可配置
		std::unique_lock<std::mutex> lock(m_navigation_mutex);
		bool ready = m_navigation_cv.wait_for(lock, std::chrono::seconds(60),
			[this]()
			{
				return m_navigation_result_ready;
			});
		if (!ready)
			return false;
		out_state = m_navigation_result;
		return true;
	}

	void handleMapInfo(const amr_interfaces::msg::MapInfo::SharedPtr msg)
	{
		bool ready = msg && msg->ready;
		if(!ready)
		{
			m_map_ready = false;
			if(m_nav2_state != Nav2State::STOPPED)
				requestStopNav2();
			return;
		}

		bool mapChanged =
			m_current_map_yaml != msg->map_yaml ||
			m_current_map_version != msg->map_version;

		if(m_nav2_pid > 0 && mapChanged)
		{
			RCLCPP_INFO(get_logger(), "map changed, restart nav2");
			requestStopNav2();
		}

		m_map_ready = true;
		m_current_map_yaml = msg->map_yaml;
		m_current_map_version = msg->map_version;

		if(m_nav2_state == Nav2State::STOPPED ||
		   m_nav2_state == Nav2State::FAILED)
		{
			RCLCPP_INFO(get_logger(), "map info ready, launching nav2");
			launchNav2Process(msg->map_yaml);
			m_nav2_state = Nav2State::STARTING_PROCESS;
		}
	}

	void handleStateMachineTick()
	{
		if(!m_map_ready)
			return;

		switch(m_nav2_state)
		{
		case Nav2State::STARTING_PROCESS:
		case Nav2State::STARTING_LIFECYCLE:
			startNav2Lifecycle();
			break;
		case Nav2State::WAITING_FOR_LOCALIZATION:
			publishInitialPoseIfReady();
			if(m_initial_pose_published)
			{
				m_nav2_state = Nav2State::READY;
				RCLCPP_INFO(get_logger(), "navigation ready");
			}
			break;
		default:
			break;
		}
	}

	void startNav2Lifecycle()
	{
		auto sendStartup =
			[this](rclcpp::Client<ManageLifecycleNodes>::SharedPtr client)
			{
				if(!client || !client->wait_for_service(std::chrono::seconds(5)))
					return false;

				auto request = std::make_shared<ManageLifecycleNodes::Request>();
				request->command = ManageLifecycleNodes::Request::STARTUP;
				client->async_send_request(request);
				return true;
			};

		bool navStarted = sendStartup(m_lifecycle_nav_client);
		bool locStarted = sendStartup(m_lifecycle_loc_client);
		if(navStarted && locStarted)
		{
			m_nav2_state = Nav2State::WAITING_FOR_LOCALIZATION;
			RCLCPP_INFO(get_logger(), "nav2 lifecycle startup requested");
		}
		else
			m_nav2_state = Nav2State::STARTING_LIFECYCLE;
	}

	void launchNav2Process(const std::string& map_yaml)
	{
		if(m_nav2_pid > 0)
			return;

		std::string paramsFile = this->get_parameter("nav2_params_file").as_string();
		bool useSimTime = this->get_parameter_or("use_sim_time", true);
		std::string robot_namespace = this->get_namespace();
		if(robot_namespace == "/")
			robot_namespace.clear();

		pid_t pid = fork();
		if(pid < 0)
		{
			m_nav2_state = Nav2State::FAILED;
			RCLCPP_ERROR(get_logger(), "fork nav2 process failed");
			return;
		}

		if(pid == 0)
		{
			setpgid(0, 0);
			std::string logPath = "/tmp/amr_navigation_logs/nav2_" + std::to_string(getpid()) + ".log";
			int logFd = open(logPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if(logFd >= 0)
			{
				dup2(logFd, STDOUT_FILENO);
				dup2(logFd, STDERR_FILENO);
				close(logFd);
			}

			execlp(
				"ros2",
				"ros2",
				"launch",
				"amr_navigation",
				"navigation2.launch.py",
				("map:=" + map_yaml).c_str(),
				("params_file:=" + paramsFile).c_str(),
				("use_sim_time:=" + std::string(useSimTime ? "true" : "false")).c_str(),
				("namespace:=" + robot_namespace).c_str(),
				"autostart:=false",
				static_cast<char*>(nullptr));
			_exit(127);
		}

		m_nav2_pid = pid;
		RCLCPP_INFO(get_logger(), "launched nav2 process pid=%d", m_nav2_pid);
	}

	void requestStopNav2()
	{
		if(m_nav2_state == Nav2State::STOPPING || m_nav2_state == Nav2State::STOPPED)
			return;

		m_nav2_state = Nav2State::STOPPING;
		stopNav2Process();
		m_nav2_state = Nav2State::STOPPED;
	}

	void stopNav2Process()
	{
		if(m_nav2_pid <= 0)
			return;

		killpg(-m_nav2_pid, SIGINT);
		for(int i = 0; i < 30; ++i)
		{
			int status = 0;
			pid_t ret = waitpid(m_nav2_pid, &status, WNOHANG);
			if(ret == m_nav2_pid || ret < 0)
				break;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		if(waitpid(m_nav2_pid, nullptr, WNOHANG) == 0)
		{
			killpg(-m_nav2_pid, SIGTERM);
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}

		if(waitpid(m_nav2_pid, nullptr, WNOHANG) == 0)
			killpg(-m_nav2_pid, SIGKILL);

		m_nav2_pid = -1;
		m_initial_pose_published = false;
	}

	void publishInitialPoseIfReady()
	{
		if(m_nav2_state != Nav2State::WAITING_FOR_LOCALIZATION &&
		   m_nav2_state != Nav2State::READY)
			return;

		if(!m_initial_pose || m_initial_pose_published)
			return;

		PoseWithCovarianceStamped msg;
		msg.header.frame_id = "map";
		msg.header.stamp = this->now();
		msg.pose.pose.position.x = m_initial_pose->x;
		msg.pose.pose.position.y = m_initial_pose->y;

		double half_yaw = m_initial_pose->yaw * 0.5;
		msg.pose.pose.orientation.z = std::sin(half_yaw);
		msg.pose.pose.orientation.w = std::cos(half_yaw);
		m_initial_pose_pub->publish(msg);
		m_initial_pose_published = true;
		RCLCPP_INFO(get_logger(), "publish initial pose x=%.2f y=%.2f yaw=%.2f",
			m_initial_pose->x, m_initial_pose->y, m_initial_pose->yaw);
	}
	
	// ---------------- members ----------------
	std::shared_ptr<TFHelper> m_tf_helper;
	std::shared_ptr<amr_navigation::NavigationManager> m_navigation_manager;
	rclcpp::Publisher<RobotPoseMsg>::SharedPtr m_robot_pose_pub;
	rclcpp::Service<GetRobotPose>::SharedPtr m_get_robot_pose_srv;
	rclcpp_action::Server<AmrNavigateToPose>::SharedPtr m_nav_action_server;
	rclcpp::TimerBase::SharedPtr m_pose_timer;

	// 导航结果通知（结果回调 -> 条件变量唤醒 execute）
	std::mutex m_navigation_mutex;
	std::condition_variable m_navigation_cv;
	bool m_navigation_result_ready{false};
	amr_navigation::NavigationExecutionState m_navigation_result{amr_navigation::NavigationExecutionState::IDLE};
	std::shared_ptr<GoalHandle> m_active_goal_handle;
	rclcpp::Subscription<amr_interfaces::msg::MapInfo>::SharedPtr m_map_info_sub;
	bool m_map_ready = false;
	rclcpp::Client<ManageLifecycleNodes>::SharedPtr m_lifecycle_nav_client;
	rclcpp::Client<ManageLifecycleNodes>::SharedPtr m_lifecycle_loc_client;
	rclcpp::Publisher<PoseWithCovarianceStamped>::SharedPtr m_initial_pose_pub;
	rclcpp::Subscription<InitialPose>::SharedPtr m_initial_pose_sub;
	InitialPose::SharedPtr m_initial_pose;
	rclcpp::TimerBase::SharedPtr m_lifecycle_retry_timer;
	pid_t m_nav2_pid = -1;
	std::string m_current_map_yaml;
	std::string m_current_map_version;
	Nav2State m_nav2_state = Nav2State::STOPPED;
	bool m_initial_pose_published = false;
};
}

int main(int argc, char ** argv)
{
	rclcpp::init(argc, argv);
	auto node = std::make_shared<amr_navigation::NavigationServer>();
	node->init();
	rclcpp::on_shutdown(
		[node]()
		{
			node->shutdownNav2Process();
		});
	rclcpp::spin(node);
	rclcpp::shutdown();
	return 0;
}
