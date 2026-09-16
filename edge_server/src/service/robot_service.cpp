#include "service/robot_service.h"
#include "robot/robot_manager.h"
#include "mqtt/mqtt_client.h"
#include "mqtt/mqtt_topic.h"

#include "nlohmann/json.hpp"
#include "utils/LogDefine.h"
#include "service/map_service.h"

using json=nlohmann::json;

namespace edge_server
{

RobotService::RobotService()
{
}

void RobotService::init(const RobotServiceRuntime& info)
{
    m_mapService = info.mapService;
    m_robotManager = info.robotManager;
}

bool RobotService::handleRegister(const RobotRegisterRequest& req, RobotRegisterResponse& resp)
{
    RobotBaseInfo robot;
    robot.robot_id = req.robot_id;
    robot.register_timestamp = req.register_timestamp;
    robot.warehouse_id = req.warehouse_id;
    robot.map_version = req.map_version;

    resp.robot_id = req.robot_id;
    resp.request_id = req.request_id;

    if(!m_robotManager->registerRobot(robot))
    {
        LOG_WARN("robot register failed id={}", robot.robot_id);
        resp.code = -1;
        resp.message = "register failed";
        return false;
    }

    MapUpdateInfo mapInfo;
    if(m_mapService->checkMapUpdate(robot.warehouse_id, robot.map_version, mapInfo))
    {
        resp.map_update = mapInfo.need_update;
        resp.map_version = mapInfo.version;
        resp.map_download_url = mapInfo.download_url;
        resp.code = 0;
    }
    else
    {
        LOG_WARN("check map failed");
        resp.code = -1;
        resp.message = "check map failed";
    }

    return true;
}

void RobotService::handleStatus(const RobotRunningStatus& status)
{
    m_robotManager->updateRobotStatus(status.robot_id, status);
}

void RobotService::handleWill(const std::string& robotId)
{
    m_robotManager->markOffline(robotId);
}

}