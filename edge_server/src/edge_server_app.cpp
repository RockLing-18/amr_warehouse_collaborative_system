#include "edge_server_app.h"
#include "context/service_context.h"
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

#include "communication/amr_communication.h"

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

    m_serviceContext = std::make_shared<ServiceContext>();
    m_serviceContext->init();

    m_webSocketServer = std::make_shared<WebSocketServer>();
    m_topicManager = std::make_shared<TopicManager>(m_webSocketServer);
    m_webSocketMessageRouter = std::make_shared<WebSocketMessageRouter>(m_topicManager);
    m_robotManager = std::make_shared<RobotManager>();
    m_robotListPublisher = std::make_shared<RobotListPublisher>(m_robotManager, m_topicManager);
    m_robotListPublisher->start(config.robot.list_period_ms);

    std::weak_ptr<RobotListPublisher> robot_publisher_weak = m_robotListPublisher;
    m_robotManager->setEventCallback(
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

    std::weak_ptr<WebSocketMessageRouter> ws_weak_router = m_webSocketMessageRouter;
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



    m_serviceContext->getMapService()->init(config.warehouse.id);

    RobotServiceRuntime runtimeInfo;
    runtimeInfo.mapService = m_serviceContext->getMapService();
    runtimeInfo.robotManager = m_robotManager;
    m_serviceContext->getRobotService()->init(runtimeInfo);

    m_serviceContext->getBootstrapService()->init(m_configManager);

    m_amrCommunication = std::make_shared<AmrCommunication>();
    m_amrCommunication->init(config.edge_amr_mqtt, m_serviceContext);
    
    m_httpServer = std::make_shared<HttpServer>(m_serviceContext->getBootstrapService(), m_serviceContext->getMapService());
    m_httpServer->start(config.http.host, config.http.port);

    LOG_INFO("edge server start");
    return true;
}

}


