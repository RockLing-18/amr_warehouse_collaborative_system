#pragma once
#include <rclcpp/rclcpp.hpp>

namespace amr_agent
{

class AmrAgentNode : public rclcpp::Node
{
public:
    explicit AmrAgentNode();
    bool init();

private:
    //void onTimerUpdateRobotModels();

private:
    // std::shared_ptr<EdgeClient> m_edge_client;
    // std::shared_ptr<GazeboClient> m_gazebo_client;
    // std::shared_ptr<AmrProcessManager> m_process_manager;
    // std::shared_ptr<RobotSyncManager> m_sync_manager;
    // std::shared_ptr<RobotLifecycleManager> m_lifecycle_manager;
    // rclcpp::TimerBase::SharedPtr m_timer;
};

}