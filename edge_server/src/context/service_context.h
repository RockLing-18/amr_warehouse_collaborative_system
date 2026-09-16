#pragma once

#include <memory>

namespace edge_server
{

// class RobotManager;
// class WebSocketServer;
// class RobotListPublisher;
// class TopicManager;
// class WebSocketMessageRouter;
// class MqttClient;
// class MqttMessageRouter;
// class RobotService;
// class HttpServer;
// class ConfigManager;
// class BootstrapService;
// class MapService;


// std::shared_ptr<RobotManager> getRobotManager();
// std::shared_ptr<WebSocketServer> getWebSocketServer();
// std::shared_ptr<RobotListPublisher> getRobotListPublisher();
// std::shared_ptr<TopicManager> getTopicManager();
// std::shared_ptr<WebSocketMessageRouter> getWebSocketMessageRouter();
// std::shared_ptr<MqttClient> getEdgeAmrMqttClient();
// std::shared_ptr<MqttMessageRouter> getEdgeAmrMqttMessageRouter();
// std::shared_ptr<RobotService> getRobotService();
// std::shared_ptr<HttpServer> getHttpServer();
// std::shared_ptr<ConfigManager> getConfigManager();
// std::shared_ptr<BootstrapService> getBootstrapService();
// std::shared_ptr<MapService> getMapService();

class RobotService;
class BootstrapService;
class MapService;

class ServiceContext
{
public:
    bool init();

public:
    std::shared_ptr<RobotService> getRobotService();
    std::shared_ptr<BootstrapService> getBootstrapService();
    std::shared_ptr<MapService> getMapService();

private:
    std::shared_ptr<RobotService> m_robotService;
    std::shared_ptr<BootstrapService> m_bootstrapService;
    std::shared_ptr<MapService> m_mapService;
};

}