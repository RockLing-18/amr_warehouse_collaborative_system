#include "robot/robot_manager.h"
#include "utils/LogDefine.h"

namespace edge_server
{

bool RobotManager::registerRobot(const RobotInfo& robot)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto iter = m_robots.find(robot.robot_id);
    if(iter != m_robots.end())
    {
        // 已存在，可以理解为重新上线
        *(iter->second) = robot;

        LOG_INFO("robot re-register, robot_id={}", robot.robot_id);
        return true;
    }

    auto info = std::make_shared<RobotInfo>(robot);
    m_robots.emplace(robot.robot_id, info);

    LOG_INFO("robot register, robot_id={}", robot.robot_id);
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
            2.0,
            3.0,
            1.57
        },
        {
            "robot02",
            "B001",
            0.0,
            0.0,
            0.0
        },
        {
            "robot03",
            "C001",
            0.0,
            1.0,
            0.0
        }
    };

    return robots;
}

}