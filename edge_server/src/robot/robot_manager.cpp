#include "robot/robot_manager.h"
#include "utils/LogDefine.h"
#include "utils/CommonFunc.h"

namespace edge_server
{
void RobotManager::setEventCallback(RobotEventCallback cb)
{
    m_callback = std::move(cb);
}

bool RobotManager::registerRobot(const RobotInfo& robot)
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
        if (iter->second.simulation_instance_id == instanceId)
            return false;

        // 已存在，可以理解为重新上线
        iter->second.simulation_instance_id = instanceId;
        LOG_INFO("robot re-register, robot_id={} simulation_instance_id={}", robot.robot_id, iter->second.simulation_instance_id);

        if(m_callback)
            m_callback(RobotEvent::REGISTER);

        return true;
    }

    RobotInfo robotInfo(robot);
    robotInfo.simulation_instance_id = instanceId;

    // 获取AMR 位置, 1.充电点  2.最后的位置
    robotInfo.pose = getRobotPose(robotInfo.robot_id);
    m_robots.emplace(robotInfo.robot_id, robotInfo);

    LOG_INFO("robot register, robot_id={} simulation_instance_id={}", robotInfo.robot_id, robotInfo.simulation_instance_id);

    if(m_callback)
        m_callback(RobotEvent::REGISTER);

    return true;
}

bool RobotManager::unregisterRobot(const std::string& robot_id)
{

}

bool RobotManager::updateRobotStatus(const std::string& robot_id, bool online)
{}

std::vector<RobotInfo> RobotManager::getRobotList()
{
    std::vector<RobotInfo> vRobot;
    std::lock_guard<std::mutex> lock(m_mutex);
    vRobot.reserve(m_robots.size());
    for(const auto& item : m_robots)
    {
        vRobot.push_back(item.second);
    }

    return vRobot;
}

RobotPose RobotManager::getRobotPose(const std::string& robot_id)
{
    RobotPose pose;
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
}

}