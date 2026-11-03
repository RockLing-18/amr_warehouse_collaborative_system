#pragma once
#include <rclcpp/rclcpp.hpp>
#include "types.h"
#include "amr_interfaces/msg/edge_status.hpp"
#include "amr_interfaces/msg/initial_pose.hpp"
#include "amr_interfaces/msg/component_status.hpp"
#include "amr_interfaces/msg/robot_pose.hpp"
#include "amr_interfaces/msg/robot_status.hpp"

namespace amr_agent
{

class EdgeCommunication;
class MapManager;
class TaskBridge;
class EdgeStateBridge;
class StatusManager;
class TrafficBridge;

class AmrAgentNode : public rclcpp::Node
{
public:
    explicit AmrAgentNode();
    bool init();

private:
    void sendRegisterRequest();
    void onRegisterResponse(const RobotRegisterResponse& response);
    void publishRobotStatus();
    void publishInitialPoseIfReady();

private:
    std::shared_ptr<EdgeCommunication> m_communication;
    std::shared_ptr<MapManager> m_mapManager;
    std::shared_ptr<TaskBridge> m_taskBridge;
    std::shared_ptr<EdgeStateBridge> m_edgeStateBridge;
    std::shared_ptr<StatusManager> m_statusManager;
    std::shared_ptr<TrafficBridge> m_trafficBridge;

    std::string m_robot_id;
    std::string m_warehouse_id;
    std::string m_map_version;
    std::string m_current_task_id;
    double m_initial_x = 0.0;
    double m_initial_y = 0.0;
    double m_initial_yaw = 0.0;
    bool m_initial_pose_published = false;
    bool m_registered = false;
    bool m_map_ready = false;
    bool m_edge_online = false;

    int m_nav_state = amr_interfaces::msg::ComponentStatus::STATE_INITIALIZING;
    int m_behavior_state = amr_interfaces::msg::ComponentStatus::STATE_INITIALIZING;
    rclcpp::Time m_nav_last_update;
    rclcpp::Time m_behavior_last_update;
    bool m_nav_has_update = false;
    bool m_behavior_has_update = false;

    rclcpp::Publisher<amr_interfaces::msg::EdgeStatus>::SharedPtr m_edge_status_pub;
    rclcpp::Publisher<amr_interfaces::msg::InitialPose>::SharedPtr m_initial_pose_pub;
    rclcpp::Subscription<amr_interfaces::msg::ComponentStatus>::SharedPtr m_nav_status_sub;
    rclcpp::Subscription<amr_interfaces::msg::ComponentStatus>::SharedPtr m_behavior_status_sub;
    rclcpp::Subscription<amr_interfaces::msg::RobotPose>::SharedPtr m_robot_pose_sub;
    rclcpp::Subscription<amr_interfaces::msg::RobotStatus>::SharedPtr m_robot_status_sub;

    double m_pose_x = 0.0;
    double m_pose_y = 0.0;
    double m_pose_yaw = 0.0;
    bool m_pose_valid = false;
    rclcpp::TimerBase::SharedPtr m_register_timer;
    rclcpp::TimerBase::SharedPtr m_heartbeat_timer;
    rclcpp::TimerBase::SharedPtr m_status_watchdog_timer;
};

}
