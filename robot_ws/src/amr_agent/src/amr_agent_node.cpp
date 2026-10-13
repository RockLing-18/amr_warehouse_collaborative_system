#include "amr_agent_node.h"
#include <chrono>
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

    m_communication = std::make_shared<EdgeCommunication>();
    if(!m_communication->init(info, shared_from_this()))
    {
        LOG_ERROR("edge communication init failed");
        return false;
    }

    m_edge_status_pub = this->create_publisher<amr_interfaces::msg::EdgeStatus>(
        amr_common::topic::EDGE_STATUS, 10);

    m_communication->setEdgeStatusCallback(
        [this](bool online)
        {
            amr_interfaces::msg::EdgeStatus msg;
            msg.online = online;
            m_edge_status_pub->publish(msg);
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
            if(m_map_ready)
                publishRobotStatus("IDLE");
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

    m_map_ready = m_mapManager && m_mapManager->isReady();
    if(m_map_ready)
        publishRobotStatus("IDLE");
}

void AmrAgentNode::publishRobotStatus(const std::string& state)
{
    if(!m_communication)
        return;

    json root;
    root["robot_id"] = m_robot_id;
    root["state"] = state;
    root["online"] = true;
    root["battery"] = 100.0;
    root["pose"]["x"] = 0.0;
    root["pose"]["y"] = 0.0;
    root["pose"]["yaw"] = 0.0;
    root["task_id"] = "";
    root["timestamp"] = utils::getCurrentTimeMs();

    m_communication->publish(
        mqtt_topic::makeRobotStatusTopic(m_robot_id),
        root.dump());
}

}
