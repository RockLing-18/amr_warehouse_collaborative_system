#include "edge_server_app.h"
#include "robot/robot_manager.h"
#include "robot/robot_list_publisher.h"
#include "websocket/websocket_server.h"
#include "websocket/websocket_message_router.h"
#include "topic/topic_manager.h"
#include "mqtt/mqtt_client.h"
#include "mqtt/mqtt_message_router.h"
#include "utils/LogDefine.h"
#include "config/config.h"
#include "mqtt/mqtt_topic.h"

namespace edge_server
{
EdgeServerApp::EdgeServerApp()
{
}

bool EdgeServerApp::init(const std::string& cfgPath)
{
    // 临时日志
    Log::init_console();

    LOG_INFO("start load config");
    if(!ConfigLoader::load(cfgPath, m_config))
    {
        LOG_ERROR("load config failed, path:{}", cfgPath);
        return false;
    }
    
    // 初始化日志
    Log::init_logger(m_config.log.level);

    LOG_INFO("load config succeed, path:{}", cfgPath);

    m_robot_manager = std::make_shared<RobotManager>();
    m_webSocketServer = std::make_shared<WebSocketServer>();
    m_topic_manager = std::make_shared<TopicManager>(m_webSocketServer);
    m_ws_router = std::make_shared<WebSocketMessageRouter>(m_topic_manager);
    m_robot_publisher = std::make_shared<RobotListPublisher>(m_robot_manager, m_topic_manager);
    m_robot_publisher->start(m_config.robot.list_period_ms);

    m_webSocketServer->setMessageCallback(
        [this] (uint64_t clientId, const std::string& msg)
        {
            m_ws_router->onMessage(clientId, msg);
        });

    if(!m_webSocketServer->start(m_config.websocket.host, m_config.websocket.port, m_config.websocket.protocol))
    {
        LOG_ERROR("websocket start failed");
        return false;
    }
    else
    {
        LOG_INFO("WebSocket server started, port={}", m_config.websocket.port);
    }

    m_edge_amr_mqtt_client = std::make_shared<MqttClient>();
    m_config.edge_amr_mqtt.will_msg_enable = true;
    m_config.edge_amr_mqtt.will.topic = mqtt_topic::EDGE_SERVER_STATUS;
    m_config.edge_amr_mqtt.will.payload = R"({"status":"offline"})";
    if(!m_edge_amr_mqtt_client->init(m_config.edge_amr_mqtt))
    {
        LOG_ERROR("mqtt init failed");
        return false;
    }

    m_edge_amr_mqtt_msg_router = std::make_shared<MqttMessageRouter>(m_robot_manager, m_edge_amr_mqtt_client);
    m_edge_amr_mqtt_msg_router->init();
    m_edge_amr_mqtt_client->setMessageCallback(
        [this](const std::string& topic, const std::string& msg)
        {
            m_edge_amr_mqtt_msg_router->onMessageProducer(topic, msg);
        });

    if(!m_edge_amr_mqtt_client->connect())
    {
        LOG_ERROR("mqtt connect failed");
        return false;
    }

    LOG_INFO("edge server start");
    return true;
}

}


