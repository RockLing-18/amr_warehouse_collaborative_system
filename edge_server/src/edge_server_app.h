#pragma once

#include <memory>

namespace edge_server
{

class ConfigManager;
class ServiceContext;
class AmrCommunication;

class RobotManager;
class WebSocketServer;
class RobotListPublisher;
class TopicManager;
class WebSocketMessageRouter;
class HttpServer;


class EdgeServerApp
{
public:
    EdgeServerApp();

    bool init(const std::string& cfgPath);

private:
    std::shared_ptr<ConfigManager> m_configManager;
    std::shared_ptr<ServiceContext> m_serviceContext;
    std::shared_ptr<AmrCommunication> m_amrCommunication;

    std::shared_ptr<RobotManager> m_robotManager;
    std::shared_ptr<WebSocketServer> m_webSocketServer;
    std::shared_ptr<RobotListPublisher> m_robotListPublisher;
    std::shared_ptr<TopicManager> m_topicManager;
    std::shared_ptr<WebSocketMessageRouter> m_webSocketMessageRouter;
    std::shared_ptr<HttpServer> m_httpServer;


};

}