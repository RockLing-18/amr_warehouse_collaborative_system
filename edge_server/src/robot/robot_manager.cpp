#include "robot/robot_manager.h"
#include "utils/LogDefine.h"

namespace edge_server
{

bool RobotManager::registerRobot(const RobotInfo& robot)
{
    if (robot.robot_id.empty())
    {
        LOG_ERROR("register robot_id is empty");
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    auto iter = m_robots.find(robot.robot_id);
    if(iter != m_robots.end())
    {
        // 存在消息重复的可能, 避免重复
        if (iter->second->simulation_instance_id == robot.register_timestamp)
            return false;

        // 已存在，可以理解为重新上线
        iter->second->simulation_instance_id = robot.register_timestamp;
        LOG_INFO("robot re-register, robot_id={} simulation_instance_id={}", robot.robot_id, iter->second->simulation_instance_id);
        return true;
    }

    RobotInfo robotInfo(robot);
    robotInfo.simulation_instance_id = robot.register_timestamp;
    auto robotInfoPtr = std::make_shared<RobotInfo>(robotInfo);
    m_robots.emplace(robotInfo.robot_id, robotInfoPtr);

    LOG_INFO("robot register, robot_id={} simulation_instance_id={}", robotInfo.robot_id, robotInfo.simulation_instance_id);
    return true;
}

bool RobotManager::unregisterRobot(const std::string& robot_id)
{

}

bool RobotManager::updateRobotStatus(const std::string& robot_id, bool online)
{}

std::vector<RobotInfo> RobotManager::getRobotList()
{
    std::vector<RobotInfo> robots = {
        {
            "robot01",
            "A001",
            "A001",
            2.0,
            3.0,
            1.57
        },
        {
            "robot02",
            "B001",
            "A001",
            0.0,
            0.0,
            0.0
        },
        {
            "robot03",
            "C001",
            "A001",
            0.0,
            1.0,
            0.0
        }
    };

    return robots;
}

}