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
#include "http/http_server.h"

#include "database/sqlite_db.h"

#include "service/robot_service.h"
#include "service/bootstrap_service.h"
#include "service/map_service.h"

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
    m_configManager = std::make_shared<ConfigManager>();
    if(!m_configManager->load(cfgPath))
    {
        LOG_ERROR("load config failed, path:{}", cfgPath);
        return false;
    }
    
    const auto& config = m_configManager->getConfig();
    // 初始化日志
    Log::init_logger(config.log.level);

    LOG_INFO("load config succeed, path:{}", cfgPath);

    auto& db = SQLiteDB::Instance();
    if(!db.Open(config.sqlite.path)) 
		return false;

    if(!db.InitTables()) 
		return false;

    m_robot_manager = std::make_shared<RobotManager>();
    m_webSocketServer = std::make_shared<WebSocketServer>();
    m_topic_manager = std::make_shared<TopicManager>(m_webSocketServer);
    m_ws_router = std::make_shared<WebSocketMessageRouter>(m_topic_manager);
    m_robot_publisher = std::make_shared<RobotListPublisher>(m_robot_manager, m_topic_manager);
    m_robot_publisher->start(config.robot.list_period_ms);

    std::weak_ptr<RobotListPublisher> robot_publisher_weak = m_robot_publisher;
    m_robot_manager->setEventCallback(
        [robot_publisher_weak](RobotManager::RobotEvent event)
        {
            LOG_DEBUG("event:{}", (int)event);
            auto publisher = robot_publisher_weak.lock();

            if(!publisher)
            {
                LOG_DEBUG("publisher is empty");
                return;
            }

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

    if(!m_webSocketServer->start(config.websocket.host, config.websocket.port, config.websocket.protocol))
    {
        LOG_ERROR("websocket start failed");
        return false;
    }
    else
    {
        LOG_INFO("WebSocket server started, port={}", config.websocket.port);
    }

    m_edge_amr_mqtt_client = std::make_shared<MqttClient>();
    if(!m_edge_amr_mqtt_client->init(config.edge_amr_mqtt))
    {
        LOG_ERROR("mqtt init failed");
        return false;
    }

    m_edge_amr_mqtt_msg_router = std::make_shared<MqttMessageRouter>();
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

    m_mapService = std::make_shared<MapService>(config.warehouse.id);
    m_robotService = std::make_shared<RobotService>(m_robot_manager, m_edge_amr_mqtt_client, m_mapService);

    // 注册处理函数
    regiestHandler();

    // 设置订阅
    setSubscribe();

    if(!m_edge_amr_mqtt_client->connect())
    {
        LOG_ERROR("mqtt connect failed");
        return false;
    }

    m_bootstrapService = std::make_shared<BootstrapService>(m_configManager);
    
    m_httpServer = std::make_shared<HttpServer>(m_bootstrapService, m_mapService);
    m_httpServer->start(config.http.host, config.http.port);

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

    std::string sAMRStatusTopic = fmt::format(mqtt_topic::ROBOT_STATUS, "+");
    m_edge_amr_mqtt_msg_router->registerHandler(
        sAMRStatusTopic, [robotService_weakPtr](const std::string& msg)
        {
            auto service = robotService_weakPtr.lock();
            if(service)
            {
                service->handleStatus(msg);
            }
        });

    std::string sAMRWillTopic = fmt::format(mqtt_topic::ROBOT_WILL, "+");
    m_edge_amr_mqtt_msg_router->registerHandler(
        sAMRWillTopic, [robotService_weakPtr](const std::string& msg)
        {
            auto service = robotService_weakPtr.lock();
            if(service)
            {
                service->handleWill(msg);
            }
        });


    // m_edge_amr_mqtt_msg_router->registerHandler(
    //     mqtt_topic::MAP_REQUEST,
    //     [this](const std::string& msg)
    //     {
    //         //m_mapService->mapDataReqHandler(msg);
    //     });

    // m_edge_amr_mqtt_msg_router->registerHandler(
    //     mqtt_topic::TRAFFIC_RIGHTS_REQ,
    //     [this](const std::string& msg)
    //     {
    //         //m_trafficService->robotRightHandler(msg);
    //     });
}

void EdgeServerApp::setSubscribe()
{
    // AMR 注册 edge server
    m_edge_amr_mqtt_client->setSubscribe(mqtt_topic::ROBOT_REGISTER_REQ, 1);

    // AMR 状态上报 (包含心跳功能)
    std::string sAMRStatusTopic = fmt::format(mqtt_topic::ROBOT_STATUS, "+");
    m_edge_amr_mqtt_client->setSubscribe(sAMRStatusTopic, 1);

    
}

}


