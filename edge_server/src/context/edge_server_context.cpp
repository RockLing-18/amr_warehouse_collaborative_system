#include "context/edge_server_context.h"
#include "robot/robot_manager.h"
#include "robot/robot_list_publisher.h"
#include "websocket/websocket_server.h"
#include "websocket/websocket_message_router.h"
#include "topic/topic_manager.h"
#include "mqtt/mqtt_client.h"
#include "mqtt/mqtt_message_router.h"
#include "config/config.h"
#include "http/http_server.h"
#include "service/robot_service.h"
#include "service/bootstrap_service.h"
#include "service/map_service.h"


namespace edge_server
{
bool EdgeServerContext::init()
{
    m_robotManager = std::make_shared<RobotManager>(this);
    m_webSocketServer = std::make_shared<WebSocketServer>();
    m_robotListPublisher = std::make_shared<RobotListPublisher>();
    m_topicManager = std::make_shared<TopicManager>();
    m_webSocketMessageRouter = std::make_shared<WebSocketMessageRouter>();
    m_edge_amr_mqttClient = std::make_shared<MqttClient>();
    m_edge_amr_mqttMessageRouter = std::make_shared<MqttMessageRouter>();
    m_robotService = std::make_shared<RobotService>();
    m_httpServer = std::make_shared<HttpServer>();
    m_configManager = std::make_shared<ConfigManager>();
    m_bootstrapService = std::make_shared<BootstrapService>();
    m_mapService = std::make_shared<MapService>();
    return true;
}

std::shared_ptr<RobotManager> EdgeServerContext::getRobotManager()
{
    return m_robotManager;
}

std::shared_ptr<WebSocketServer> EdgeServerContext::getWebSocketServer()
{
    return m_webSocketServer;
}

std::shared_ptr<RobotListPublisher> EdgeServerContext::getRobotListPublisher()
{
    return m_robotListPublisher;
}

std::shared_ptr<TopicManager> EdgeServerContext::getTopicManager()
{
    return m_topicManager;
}

std::shared_ptr<WebSocketMessageRouter> EdgeServerContext::getWebSocketMessageRouter()
{
    return m_webSocketMessageRouter;
}

std::shared_ptr<MqttClient> EdgeServerContext::getEdgeAmrMqttClient()
{
    return m_edge_amr_mqttClient;
}

std::shared_ptr<MqttMessageRouter> EdgeServerContext::getEdgeAmrMqttMessageRouter()
{
    return m_edge_amr_mqttMessageRouter;
}

std::shared_ptr<RobotService> EdgeServerContext::getRobotService()
{
    return m_robotService;
}

std::shared_ptr<HttpServer> EdgeServerContext::getHttpServer()
{
    return m_httpServer;
}

std::shared_ptr<ConfigManager> EdgeServerContext::getConfigManager()
{
    return m_configManager;
}

std::shared_ptr<BootstrapService> EdgeServerContext::getBootstrapService()
{
    return m_bootstrapService;
}

std::shared_ptr<MapService> EdgeServerContext::getMapService()
{
    return m_mapService;
}


}