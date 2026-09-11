#include "robot/robot_manager.h"
#include "utils/LogDefine.h"
#include "utils/CommonFunc.h"


namespace edge_server
{
void RobotManager::setEventCallback(RobotEventCallback cb)
{
    m_callback = std::move(cb);
}

bool RobotManager::registerRobot(const RobotBaseInfo& robot)
{
    if (robot.robot_id.empty())
    {
        LOG_ERROR("register robot_id is empty");
        return false;
    }

    std::string instanceId = utils::ms_to_md_hms(robot.register_timestamp);
    std::lock_guard<std::mutex> lock(m_mutex);
    auto iter = m_robots.find(robot.robot_id);
    if(iter != m_robots.end())
    {
        // 存在消息重复的可能, 避免重复
        if (iter->second.getInstanceInfo().simulation_instance_id == instanceId)
            return false;

        // // 已存在，可以理解为重新上线
        // RobotBaseInfo robotNew;
        // robotNew = robot;
        // robotNew.simulation_instance_id = instanceId;
        // iter->second.updateBaseInfo(robotNew);

        // LOG_INFO("robot re-register, robot_id={} simulation_instance_id={}", robot.robot_id, instanceId);

        // if(m_callback)
        //     m_callback(RobotEvent::REGISTER);

        // return true;
    }

    RobotEntity robotEntity;
    RobotBaseInfo robotNew(robot);
    robotNew.simulation_instance_id = instanceId;
    robotEntity.updateBaseInfo(robotNew);
    RobotRunningStatus robotStatus;
    robotStatus.state = RobotState::INITIALIZING;
    robotStatus.online = true;
    robotStatus.timestamp = utils::getCurrentTime();

    // 获取AMR 位置, 1.充电点  2.最后的位置
    robotStatus.pose = getRobotPose(robotNew.robot_id);
    robotEntity.updateStatus(robotStatus);
    m_robots[robotNew.robot_id] = robotEntity;

    LOG_INFO("robot register, robot_id={} simulation_instance_id={}", robotNew.robot_id, robotNew.simulation_instance_id);

    if(m_callback)
        m_callback(RobotEvent::REGISTER);

    return true;
}

bool RobotManager::unregisterRobot(const std::string& robot_id)
{

}

bool RobotManager::markOffline(const std::string& robot_id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto iter = m_robots.find(robot_id);
    if(iter == m_robots.end())
    {
        LOG_ERROR("not found robot_id:{}", robot_id);
        return false;
    }

    auto status = iter->second.getRunningStatus();
    status.online = false;
    status.state = RobotState::OFFLINE;
    status.timestamp = utils::getCurrentTime();
    iter->second.updateStatus(status);
    return true;
}

bool RobotManager::updateRobotStatus(const std::string& robot_id, const RobotRunningStatus& status)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto iter = m_robots.find(robot_id);
    if (iter == m_robots.end())
    {
        LOG_ERROR("not found robot_id:{}", robot_id);
        return false;
    }

    iter->second.updateStatus(status);
    return true;
}

std::vector<RobotInstanceInfo> RobotManager::getRobotList()
{
    std::vector<RobotInstanceInfo> vRobot;
    std::lock_guard<std::mutex> lock(m_mutex);
    vRobot.reserve(m_robots.size());
    for(const auto& item : m_robots)
    {
        vRobot.emplace_back(item.second.getInstanceInfo());
    }

    return vRobot;
}

Pose2D RobotManager::getRobotPose(const std::string& robot_id)
{
    Pose2D pose;
    if(robot_id == "robot001")
    {
        pose.x = 0.0;
        pose.y = 0.0;
        pose.yaw = 1.57;
    }
    else if(robot_id == "robot002")
    {
        pose.x = 1.0;
        pose.y = 1.0;
        pose.yaw = 0;
    }

    return pose;
}

}