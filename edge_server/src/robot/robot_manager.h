#pragma once

#include "types.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <functional>
#include "robot/robot_entity.h"

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
    bool registerRobot(const RobotBaseInfo& robot);

    // 启动时从数据库恢复机器人
    void restoreRobot(const RobotBaseInfo& robot, const RobotRunningStatus& status);

    // 获取机器人基础信息
    bool getRobotBaseInfo(const std::string& robot_id, RobotBaseInfo& robot);

    // 注销机器人
    bool unregisterRobot(const std::string& robot_id);

    bool markOffline(const std::string& robot_id);

    // 更新机器人状态
    bool updateRobotStatus(const std::string& robot_id, const RobotRunningStatus& status);
    bool getRobotStatus(const std::string& robot_id, RobotRunningStatus& status);

    // 获取所有机器人 推送给仿真管理服务
    std::vector<RobotInstanceInfo> getRobotList();

    // 初始点位分配器（IDLE 充电点）
    using PoseAllocator = std::function<bool(Pose2D&)>;
    void setPoseAllocator(PoseAllocator allocator);

private:
    RobotEventCallback m_callback;
    PoseAllocator m_poseAllocator;
    std::unordered_map<std::string, RobotEntity> m_robots;
    std::mutex m_mutex;
};


}
