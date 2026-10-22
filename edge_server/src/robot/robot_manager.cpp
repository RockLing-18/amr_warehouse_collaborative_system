#include "robot/robot_manager.h"
#include "utils/LogDefine.h"
#include "utils/CommonFunc.h"


namespace edge_server
{
void RobotManager::setEventCallback(RobotEventCallback cb)
{
    m_callback = std::move(cb);
}

void RobotManager::setPoseAllocator(PoseAllocator allocator)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_poseAllocator = std::move(allocator);
}

bool RobotManager::registerRobot(const RobotBaseInfo& robot)
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
        // 已存在（含重启后从数据库恢复的机器人）：保留原 instance_id，
        // 仅刷新基础信息，避免 instance_id 变化导致仿真侧误判需要重建模型。
        RobotBaseInfo robotNew(robot);
        robotNew.instance_id = iter->second.getInstanceInfo().instance_id;
        iter->second.updateBaseInfo(robotNew);

        LOG_INFO("robot re-register, robot_id={} instance_id={}",
            robotNew.robot_id, robotNew.instance_id);

        if(m_callback)
            m_callback(RobotEvent::REGISTER);

        return true;
    }

    std::string instanceId = utils::ms_to_md_hms(robot.register_timestamp);
    RobotEntity robotEntity;
    RobotBaseInfo robotNew(robot);
    robotNew.instance_id = instanceId;
    robotEntity.updateBaseInfo(robotNew);
    RobotRunningStatus robotStatus;
    robotStatus.state = RobotState::INITIALIZING;
    robotStatus.online = true;
    robotStatus.timestamp = utils::getCurrentTime();

    // 获取AMR 初始点位：优先分配 IDLE 充电点，失败回退到原点
    robotStatus.pose = Pose2D{};
    if(m_poseAllocator)
        m_poseAllocator(robotStatus.pose);
    robotEntity.updateStatus(robotStatus);
    m_robots[robotNew.robot_id] = robotEntity;

    LOG_INFO("robot register, robot_id={} instance_id={}", robotNew.robot_id, robotNew.instance_id);

    if(m_callback)
        m_callback(RobotEvent::REGISTER);

    return true;
}

void RobotManager::restoreRobot(const RobotBaseInfo& robot, const RobotRunningStatus& status)
{
    if(robot.robot_id.empty())
        return;

    std::lock_guard<std::mutex> lock(m_mutex);
    if(m_robots.find(robot.robot_id) != m_robots.end())
        return;

    RobotEntity entity;
    entity.updateBaseInfo(robot);

    RobotRunningStatus restored = status;
    restored.robot_id = robot.robot_id;
    restored.online = false;
    restored.state = RobotState::OFFLINE;
    entity.updateStatus(restored);

    m_robots[robot.robot_id] = entity;
    LOG_INFO("robot restored from db, robot_id={} instance_id={}",
        robot.robot_id.c_str(), robot.instance_id.c_str());
}

bool RobotManager::getRobotBaseInfo(const std::string& robot_id, RobotBaseInfo& robot)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto iter = m_robots.find(robot_id);
    if(iter == m_robots.end())
        return false;

    robot = iter->second.getBaseInfo();
    return true;
}

bool RobotManager::unregisterRobot(const std::string& robot_id)
{
    return true;
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

bool RobotManager::getRobotStatus(const std::string& robot_id, RobotRunningStatus& status)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto iter = m_robots.find(robot_id);
    if(iter == m_robots.end())
        return false;

    status = iter->second.getRunningStatus();
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

}
