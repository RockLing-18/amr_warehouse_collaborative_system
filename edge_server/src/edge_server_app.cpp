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

#include "service/robot_service.h"

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

    std::weak_ptr<RobotListPublisher> robot_publisher_weak = m_robot_publisher;
    m_robot_manager->setEventCallback(
        [robot_publisher_weak](RobotManager::RobotEvent event)
        {
            auto publisher = robot_publisher_weak.lock();

            if(!publisher)
                return;

            switch(event)
            {
            case RobotManager::RobotEvent::REGISTER:
            case RobotManager::RobotEvent::OFFLINE:
                publisher->triggerPublish();
                break;
            case RobotManager::RobotEvent::STATUS_CHANGED:
                break;
            case RobotManager::RobotEvent::POSE_CHANGED:
                break;
            }
        });

    std::weak_ptr<WebSocketMessageRouter> ws_weak_router = m_ws_router;
    m_webSocketServer->setMessageCallback(
        [ws_weak_router] (uint64_t clientId, const std::string& msg)
        {
            auto router = ws_weak_router.lock();
            if(router)
            {
                router->onMessage(clientId, msg);
            }
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

    m_edge_amr_mqtt_msg_router = std::make_shared<MqttMessageRouter>();
    m_robotService = std::make_shared<RobotService>(m_robot_manager, m_edge_amr_mqtt_client);
    regiestHandler();

    std::weak_ptr<MqttMessageRouter> edge_amr_mqtt_msg_weak_router = m_edge_amr_mqtt_msg_router;
    m_edge_amr_mqtt_client->setMessageCallback(
        [edge_amr_mqtt_msg_weak_router](const std::string& topic, const std::string& msg)
        {
            auto router = edge_amr_mqtt_msg_weak_router.lock();
            if(router)
            {
                router->onMessageProducer(topic, msg);
            }
        });

    m_edge_amr_mqtt_client->setSubscribe(mqtt_topic::ROBOT_REGISTER_REQ, 1);
    
    std::string sAMRStatusTopic = fmt::format(mqtt_topic::ROBOT_STATUS, "+");
    m_edge_amr_mqtt_client->setSubscribe(sAMRStatusTopic, 1);
    
    if(!m_edge_amr_mqtt_client->connect())
    {
        LOG_ERROR("mqtt connect failed");
        return false;
    }

    LOG_INFO("edge server start");
    return true;
}

void EdgeServerApp::regiestHandler()
{
    std::weak_ptr<RobotService> robotService_weakPtr = m_robotService;
    m_edge_amr_mqtt_msg_router->registerHandler(
        mqtt_topic::ROBOT_REGISTER_REQ, [robotService_weakPtr](const std::string& msg)
        {
            auto service = robotService_weakPtr.lock();
            if(service)
            {
                service->handleRegister(msg);
            }
        });

    m_edge_amr_mqtt_msg_router->registerHandler(
        mqtt_topic::MAP_REQUEST,
        [this](const std::string& msg)
        {
            //m_mapService->mapDataReqHandler(msg);
        });

    m_edge_amr_mqtt_msg_router->registerHandler(
        mqtt_topic::TRAFFIC_RIGHTS_REQ,
        [this](const std::string& msg)
        {
            //m_trafficService->robotRightHandler(msg);
        });
}

}


