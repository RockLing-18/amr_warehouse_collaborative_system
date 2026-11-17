#include "amr_agent_node.h"
#include <chrono>
#include <algorithm>
#include <cmath>
#include "types.h"
#include "bootstrap/bootstrap.h"
#include "communication/edge_communication.h"
#include "bridge/task_bridge.h"
#include "map/map_manager.h"
#include "utils/ros_logger.h"
#include "nlohmann/json.hpp"
#include "mqtt/mqtt_topic.h"
#include "utils/CommonFunc.h"
#include "amr_common/ros_interface_names.h"
#include "amr_common/robot_state.h"

using json = nlohmann::json;

namespace amr_agent
{

AmrAgentNode::AmrAgentNode()
: Node("amr_agent")
{
    this->declare_parameter<std::string>("cfg_path","");
    this->declare_parameter<std::string>(
        "pending_report_path",
        "/opt/amr/data/amr_runtime/pending_task_reports.json");
}


bool AmrAgentNode::init()
{
    GlobalRosLogger::init(this->get_logger(), rclcpp::Logger::Level::Debug);

    std::string cfgPath;
    this->get_parameter("cfg_path", cfgPath);
    if(cfgPath.empty())
    {
        cfgPath = "/opt/amr/config/amr_config.yaml";
    }
    
    Bootstrap bootstrap;
    BootstrapInfo info;
    if(!bootstrap.run(cfgPath, info))
    {
        LOG_ERROR("bootstrap failed");
        return false;
    }

    m_robot_id = info.robot_id;
    m_warehouse_id = info.warehouse_id;
    m_map_version = info.map.version;
    m_initial_x = info.initial_x;
    m_initial_y = info.initial_y;
    m_initial_yaw = info.initial_yaw;

    m_communication = std::make_shared<EdgeCommunication>();
    if(!m_communication->init(info, shared_from_this()))
    {
        LOG_ERROR("edge communication init failed");
        return false;
    }

    m_edge_status_pub = this->create_publisher<amr_interfaces::msg::EdgeStatus>(
        amr_common::topic::EDGE_STATUS, 10);

    auto initial_pose_qos = rclcpp::QoS(rclcpp::KeepLast(1));
    initial_pose_qos.reliable();
    initial_pose_qos.transient_local();
    m_initial_pose_pub = this->create_publisher<amr_interfaces::msg::InitialPose>(
        amr_common::topic::INITIAL_POSE, initial_pose_qos);

    m_nav_status_sub = this->create_subscription<amr_interfaces::msg::ComponentStatus>(
        amr_common::topic::NAVIGATION_STATUS,
        10,
        [this](const amr_interfaces::msg::ComponentStatus::SharedPtr msg)
        {
            if(!msg)
                return;
            m_nav_state = msg->state;
            m_nav_last_update = this->now();
            m_nav_has_update = true;
        });

    m_behavior_status_sub = this->create_subscription<amr_interfaces::msg::ComponentStatus>(
        amr_common::topic::BEHAVIOR_STATUS,
        10,
        [this](const amr_interfaces::msg::ComponentStatus::SharedPtr msg)
        {
            if(!msg)
                return;
            m_behavior_state = msg->state;
            m_behavior_last_update = this->now();
            m_behavior_has_update = true;
        });

    m_robot_status_sub = this->create_subscription<amr_interfaces::msg::RobotStatus>(
        amr_common::topic::ROBOT_STATUS,
        10,
        [this](const amr_interfaces::msg::RobotStatus::SharedPtr msg)
        {
            if(!msg)
                return;
            m_current_task_id = msg->current_task_id;
            m_behavior_navigating = msg->navigating;
        });

    m_robot_pose_sub = this->create_subscription<amr_interfaces::msg::RobotPose>(
        amr_common::topic::ROBOT_POSE,
        10,
        [this](const amr_interfaces::msg::RobotPose::SharedPtr msg)
        {
            if(!msg)
                return;
            m_pose_valid = msg->pose_valid;
            if(m_pose_valid)
            {
                m_pose_x = msg->pose.pose.position.x;
                m_pose_y = msg->pose.pose.position.y;

                const auto& q = msg->pose.pose.orientation;
                double siny_cosp = 2.0 * (q.w * q.z + q.x * q.y);
                double cosy_cosp = 1.0 - 2.0 * (q.y * q.y + q.z * q.z);
                m_pose_yaw = std::atan2(siny_cosp, cosy_cosp);
            }
        });

    m_status_watchdog_timer = this->create_wall_timer(
        std::chrono::seconds(1),
        [this]()
        {
            auto now = this->now();
            constexpr double kStaleSeconds = 3.0;

            if(m_nav_has_update &&
               (now - m_nav_last_update).seconds() > kStaleSeconds)
            {
                m_nav_state = amr_interfaces::msg::ComponentStatus::STATE_ERROR;
            }

            if(m_behavior_has_update &&
               (now - m_behavior_last_update).seconds() > kStaleSeconds)
            {
                m_behavior_state = amr_interfaces::msg::ComponentStatus::STATE_ERROR;
            }
        });

    m_communication->setEdgeStatusCallback(
        [this](bool online)
        {
            amr_interfaces::msg::EdgeStatus msg;
            msg.online = online;
            m_edge_status_pub->publish(msg);

            bool wasOnline = m_edge_online;
            m_edge_online = online;
            if(online && !wasOnline)
            {
                LOG_INFO("edge recovered, re-register robot");
                m_registered = false;
                sendRegisterRequest();
            }
        });

    m_mapManager = std::make_shared<MapManager>(shared_from_this());
    if(!m_mapManager->init(info.map.path, info.edge_host, info.edge_port))
    {
        LOG_ERROR("map manager init failed");
        return false;
    }

    m_taskBridge = std::make_shared<TaskBridge>(
        shared_from_this(),
        m_communication.get(),
        info.robot_id,
        info.warehouse_id,
        this->get_parameter("pending_report_path").as_string());
    m_taskBridge->init();

    m_communication->setRegisterResponseCallback(
        [this](const RobotRegisterResponse& response)
        {
            onRegisterResponse(response);
        });

    sendRegisterRequest();

    m_register_timer = this->create_wall_timer(
        std::chrono::seconds(5),
        [this]()
        {
            if(!m_registered)
                sendRegisterRequest();
        });

    m_heartbeat_timer = this->create_wall_timer(
        std::chrono::seconds(5),
        [this]()
        {
            if(m_registered)
                publishRobotStatus();
        });

    return true;
}

void AmrAgentNode::sendRegisterRequest()
{
    if(!m_communication)
        return;

    json root;
    root["robot_id"] = m_robot_id;
    root["warehouse_id"] = m_warehouse_id;
    root["map_version"] = m_map_version;
    root["request_id"] = m_robot_id + "_" + std::to_string(utils::getCurrentTimeMs());
    root["timestamp"] = utils::getCurrentTimeMs();

    m_communication->publish(
        mqtt_topic::ROBOT_REGISTER_REQ,
        root.dump());

    LOG_INFO("send register request robot=%s", m_robot_id.c_str());
}

void AmrAgentNode::onRegisterResponse(const RobotRegisterResponse& response)
{
    LOG_INFO(
        "receive register response robot=%s code=%d map_update=%s",
        m_robot_id.c_str(),
        response.code,
        response.map_update ? "true" : "false");

    if(m_mapManager)
        m_mapManager->onRegisterResponse(response);

    if(response.code == 0)
        m_registered = true;

    if(response.has_initial_pose)
    {
        m_initial_x = response.initial_x;
        m_initial_y = response.initial_y;
        m_initial_yaw = response.initial_yaw;
        LOG_INFO(
            "receive initial pose x=%.2f y=%.2f yaw=%.2f",
            m_initial_x, m_initial_y, m_initial_yaw);
    }

    m_map_ready = m_mapManager && m_mapManager->isReady();
    if(m_map_ready)
    {
        publishInitialPoseIfReady();
        publishRobotStatus();
    }
}

void AmrAgentNode::publishInitialPoseIfReady()
{
    if(m_initial_pose_published || !m_mapManager || !m_mapManager->isReady())
        return;

    amr_interfaces::msg::InitialPose msg;
    msg.x = m_initial_x;
    msg.y = m_initial_y;
    msg.yaw = m_initial_yaw;
    m_initial_pose_pub->publish(msg);
    m_initial_pose_published = true;
    LOG_INFO("publish initial pose x=%.2f y=%.2f yaw=%.2f", msg.x, msg.y, msg.yaw);
}

void AmrAgentNode::publishRobotStatus()
{
    if(!m_communication)
        return;

    int aggregated;
    if(m_behavior_state == amr_interfaces::msg::ComponentStatus::STATE_IDLE &&
       m_behavior_navigating)
    {
        // 返航期间行为模块已可接新任务，对外保持 IDLE，移动由 navigating 字段表达
        aggregated = amr_interfaces::msg::ComponentStatus::STATE_IDLE;
    }
    else
    {
        aggregated = std::min(m_nav_state, m_behavior_state);
    }
    const char* state = amr_common::robotStateToString(
        static_cast<amr_common::RobotState>(aggregated));

    json root;
    root["robot_id"] = m_robot_id;
    root["state"] = state;
    root["online"] = true;
    root["battery"] = 100.0;
    root["pose"]["x"] = m_pose_valid ? m_pose_x : 0.0;
    root["pose"]["y"] = m_pose_valid ? m_pose_y : 0.0;
    root["pose"]["yaw"] = m_pose_valid ? m_pose_yaw : 0.0;
    root["task_id"] = m_current_task_id;
    root["timestamp"] = utils::getCurrentTimeMs();

    m_communication->publish(
        mqtt_topic::makeRobotStatusTopic(m_robot_id),
        root.dump());
}

}
