#pragma once
#include <rclcpp/rclcpp.hpp>
#include "types.h"
#include "amr_interfaces/msg/edge_status.hpp"
#include "amr_interfaces/msg/initial_pose.hpp"
#include "amr_interfaces/msg/component_status.hpp"

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
    double m_initial_x = 0.0;
    double m_initial_y = 0.0;
    double m_initial_yaw = 0.0;
    bool m_initial_pose_published = false;
    bool m_registered = false;
    bool m_map_ready = false;
    bool m_edge_online = false;

    int m_nav_state = amr_interfaces::msg::ComponentStatus::STATE_INITIALIZING;
    int m_behavior_state = amr_interfaces::msg::ComponentStatus::STATE_INITIALIZING;

    rclcpp::Publisher<amr_interfaces::msg::EdgeStatus>::SharedPtr m_edge_status_pub;
    rclcpp::Publisher<amr_interfaces::msg::InitialPose>::SharedPtr m_initial_pose_pub;
    rclcpp::Subscription<amr_interfaces::msg::ComponentStatus>::SharedPtr m_nav_status_sub;
    rclcpp::Subscription<amr_interfaces::msg::ComponentStatus>::SharedPtr m_behavior_status_sub;
    rclcpp::TimerBase::SharedPtr m_register_timer;
    rclcpp::TimerBase::SharedPtr m_heartbeat_timer;
};

}
