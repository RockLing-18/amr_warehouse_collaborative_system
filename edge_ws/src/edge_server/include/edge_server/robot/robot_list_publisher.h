#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include <chrono>

namespace edge_server
{

class RobotManager;
class TopicManager;

class RobotListPublisher
{

public:
    RobotListPublisher(const std::shared_ptr<RobotManager>& robot_manager, const std::shared_ptr<TopicManager>& topicManager);
    ~RobotListPublisher();
    
    void start(int period_ms);
    void stop();

private:
    void publish();

private:
    std::shared_ptr<RobotManager> m_robot_manager;
    std::shared_ptr<TopicManager> m_topicManager;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    int m_period_ms{1000};
};

}