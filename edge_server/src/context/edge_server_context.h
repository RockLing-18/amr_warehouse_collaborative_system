#pragma once

#include <memory>

namespace edge_server
{

class RobotManager;
class WebSocketServer;
class RobotListPublisher;
class TopicManager;
class WebSocketMessageRouter;
class MqttClient;
class MqttMessageRouter;
class RobotService;
class HttpServer;
class ConfigManager;
class BootstrapService;
class MapService;

class EdgeServerContext
{

public:
    bool init();

public:
    std::shared_ptr<RobotManager> getRobotManager();
    std::shared_ptr<WebSocketServer> getWebSocketServer();
    std::shared_ptr<RobotListPublisher> getRobotListPublisher();
    std::shared_ptr<TopicManager> getTopicManager();
    std::shared_ptr<WebSocketMessageRouter> getWebSocketMessageRouter();
    std::shared_ptr<MqttClient> getEdgeAmrMqttClient();
    std::shared_ptr<MqttMessageRouter> getEdgeAmrMqttMessageRouter();
    std::shared_ptr<RobotService> getRobotService();
    std::shared_ptr<HttpServer> getHttpServer();
    std::shared_ptr<ConfigManager> getConfigManager();
    std::shared_ptr<BootstrapService> getBootstrapService();
    std::shared_ptr<MapService> getMapService();

private:
    std::shared_ptr<RobotManager> m_robotManager;
    std::shared_ptr<WebSocketServer> m_webSocketServer;
    std::shared_ptr<RobotListPublisher> m_robotListPublisher;
    std::shared_ptr<TopicManager> m_topicManager;
    std::shared_ptr<WebSocketMessageRouter> m_webSocketMessageRouter;
    std::shared_ptr<MqttClient> m_edge_amr_mqttClient;
    std::shared_ptr<MqttMessageRouter> m_edge_amr_mqttMessageRouter;
    std::shared_ptr<RobotService> m_robotService;
    std::shared_ptr<HttpServer> m_httpServer;
    std::shared_ptr<ConfigManager> m_configManager;
    std::shared_ptr<BootstrapService> m_bootstrapService;
    std::shared_ptr<MapService> m_mapService;
};

}