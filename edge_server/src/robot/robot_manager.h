#pragma once

#include "types.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <vector>

namespace edge_server
{

class RobotManager
{
public:
    RobotManager() = default;
    ~RobotManager() = default;

public:
    // 注册机器人
    bool registerRobot(const RobotInfo& robot);

    // 注销机器人
    bool unregisterRobot(const std::string& robot_id);

    // 更新机器人状态
    bool updateRobotStatus(const std::string& robot_id, bool online);

    // 获取所有机器人
    std::vector<RobotInfo> getRobotList();

private:
    std::unordered_map<std::string, std::shared_ptr<RobotInfo>> m_robots;
    std::mutex m_mutex;
};


}