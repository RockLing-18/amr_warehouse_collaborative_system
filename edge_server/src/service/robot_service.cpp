#include "service/robot_service.h"
#include "robot/robot_manager.h"
#include "mqtt/mqtt_client.h"
#include "mqtt/mqtt_topic.h"

#include "nlohmann/json.hpp"
#include "utils/LogDefine.h"

using json=nlohmann::json;

namespace edge_server
{

RobotService::RobotService(const std::shared_ptr<RobotManager>& robotManager, const std::shared_ptr<MqttClient>& edgeAmrMqttClient)
: m_robotManager(robotManager), m_edgeAmrMqttClient(edgeAmrMqttClient)
{
}

void RobotService::handleRegister(const std::string& message)
{
    try
    {
        auto root = json::parse(message);
        if(!root.contains("robot_id"))
        {
            LOG_ERROR("register missing robot_id");
            return;
        }

        if(!root.contains("timestamp"))
        {
            LOG_ERROR("register missing timestamp");
            return;
        }

        if(!root.contains("request_id"))
        {
            LOG_ERROR("register missing request_id");
            return;
        }

        RobotBaseInfo robot;
        robot.robot_id = root["robot_id"].get<std::string>();
        robot.register_timestamp = root["timestamp"].get<uint64_t>();
        std::string requestId = root["request_id"].get<std::string>();
        if(!m_robotManager->registerRobot(robot))
        {
            LOG_WARN("robot register failed id={}", robot.robot_id);
            return;
        }

        sendRegisterResponse(robot.robot_id, requestId);
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("robot register exception:{}", e.what());
    }
}

void RobotService::sendRegisterResponse(const std::string& robotId, const std::string& requestId)
{
    if(!m_edgeAmrMqttClient)
    {
        LOG_ERROR("mqtt client null");
        return;
    }

    std::string topic = mqtt_topic::ROBOT_REGISTER_RSP_PREFIX + robotId;

    json rsp;
    rsp["robot_id"] = robotId;
    rsp["request_id"] = requestId;
    rsp["map_download_url"] = "http://192.168.1.95/map/data";

    bool ok = m_edgeAmrMqttClient->publish(topic, rsp.dump());
    LOG_INFO("register response robot={}, result={}", robotId, ok);
}

void RobotService::handleStatus(const std::string& message)
{
    try
    {
        auto root = json::parse(message);
        if(!root.contains("robot_id"))
        {
            LOG_ERROR("missing robot_id");
            return;
        }

        if(!root.contains("timestamp"))
        {
            LOG_ERROR("missing timestamp");
            return;
        }

        if(!root.contains("state"))
        {
            LOG_ERROR("missing state");
            return;
        }

        if(!root.contains("pose"))
        {
            LOG_ERROR("missing pose");
            return;
        }

        RobotRunningStatus status;
        std::string robotId = root["robot_id"].get<std::string>();
        status.timestamp = root["timestamp"].get<uint64_t>();
        status.state = RobotStateFromString(root["state"].get<std::string>());
        status.online = true;
        status.battery = root.value("battery", 0.0);
        status.pose.x = root["pose"]["x"].get<double>();
        status.pose.y = root["pose"]["y"].get<double>();
        status.pose.yaw = root["pose"]["yaw"].get<double>();
        status.task_id = root.value("task_id", "");

        m_robotManager->updateRobotStatus(robotId, status);
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("handleStatus exception:{}", e.what());
    }
}

void RobotService::handleWill(const std::string& message)
{
    try
    {
        auto root = json::parse(message);
        if(!root.contains("robot_id"))
        {
            LOG_ERROR("missing robot_id");
            return;
        }

        std::string robotId = root["robot_id"].get<std::string>();
        m_robotManager->markOffline(robotId);
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("handleWill exception:{}", e.what());
    }
}

}