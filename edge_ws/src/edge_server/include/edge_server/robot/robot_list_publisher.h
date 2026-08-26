#pragma once

#include "edge_server/robot/robot_manager.h"
#include "edge_server/websocket/websocket_server.h"
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>

namespace edge_server
{

class RobotListPublisher
{

public:
    RobotListPublisher(const std::shared_ptr<RobotManager>& robot_manager, const std::shared_ptr<WebSocketServer>& websocket);
    ~RobotListPublisher();
    
    void start(int period_ms);
    void stop();

private:
    void publish();

private:
    std::shared_ptr<RobotManager> m_robot_manager;
    std::shared_ptr<WebSocketServer> m_websocket;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    int m_period_ms{1000};
};

}