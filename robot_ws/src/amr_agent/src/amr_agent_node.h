#pragma once
#include <rclcpp/rclcpp.hpp>

namespace amr_agent
{

class EdgeCommunication;
class MapManager;
class TaskBridge;
class EdgeStateBridge;
class StatusManager;
class TrafficBridge;

class AmrAgentNode : public rclcpp::Node
{
public:
    explicit AmrAgentNode();
    bool init();

private:
    //void onTimerUpdateRobotModels();

private:
    std::shared_ptr<EdgeCommunication> m_communication;
    std::shared_ptr<MapManager> m_mapManager;
    std::shared_ptr<TaskBridge> m_taskBridge;
    std::shared_ptr<EdgeStateBridge> m_edgeStateBridge;
    std::shared_ptr<StatusManager> m_statusManager;

    // rclcpp::TimerBase::SharedPtr m_timer;
};

}