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

        std::string robotId = root["robot_id"].get<std::string>();
        uint64_t timestamp = root["timestamp"].get<uint64_t>();
        std::string requestId = root["request_id"].get<std::string>();

        RobotInfo robot;
        robot.robot_id = robotId;
        robot.register_timestamp = timestamp;
        if(!m_robotManager->registerRobot(robot))
        {
            LOG_WARN("robot register failed id={}", robotId);
            return;
        }

        sendRegisterResponse( robotId, requestId);
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


}