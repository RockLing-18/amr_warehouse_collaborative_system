#pragma once

#include <memory>
#include "config/config.h"

namespace edge_server
{

class RobotManager;
class WebSocketServer;
class RobotListPublisher;
class TopicManager;
class WebSocketMessageRouter;

class EdgeServerApp
{
public:
    EdgeServerApp();

    bool init(const std::string& cfgPath);
private:
    std::shared_ptr<RobotManager> m_robot_manager;
    std::shared_ptr<WebSocketServer> m_webSocketServer;
    std::shared_ptr<RobotListPublisher> m_robot_publisher;
    std::shared_ptr<TopicManager> m_topic_manager;
    std::shared_ptr<WebSocketMessageRouter> m_ws_router;
    Config m_config;
};

}