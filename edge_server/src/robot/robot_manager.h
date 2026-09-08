#pragma once

#include "types.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <functional>

namespace edge_server
{

class RobotManager
{
public:
    enum class RobotEvent
    {
        REGISTER,
        STATUS_CHANGED,
        POSE_CHANGED,
        OFFLINE
    };

    using RobotEventCallback = std::function<void(RobotEvent)>;

public:
    RobotManager() = default;
    ~RobotManager() = default;


public:
    void setEventCallback(RobotEventCallback cb);

    // 注册机器人
    bool registerRobot(const RobotInfo& robot);

    // 注销机器人
    bool unregisterRobot(const std::string& robot_id);

    // 更新机器人状态
    bool updateRobotStatus(const std::string& robot_id, bool online);

    // 获取所有机器人
    std::vector<RobotInfo> getRobotList();

    // 获取机器人位置
    RobotPose getRobotPose(const std::string& robot_id);

private:
    RobotEventCallback m_callback;
    std::unordered_map<std::string, RobotInfo> m_robots;
    std::mutex m_mutex;
};


}