#include "communication/edge_communication.h"
#include "mqtt/mqtt_client.h"
#include "mqtt/mqtt_topic.h"
#include "mqtt/mqtt_message_router.h"
// #include "context/service_context.h"
// #include "service/robot_service.h"
#include "message/message_codec.h"
#include "utils/ros_logger.h"
#include <fmt/format.h>

namespace amr_agent
{
EdgeCommunication::EdgeCommunication()
{
    m_mqtt = std::make_unique<MqttClient>();
    m_router =  std::make_unique<MqttMessageRouter>();
}

bool EdgeCommunication::init(const BootstrapInfo& cfg, const std::shared_ptr<ServiceContext>& serviceContext)
{
    // m_context = serviceContext;
    m_robotId = cfg.robot_id;
    MqttConfig config = cfg.mqtt;
    config.will_msg_enable = true;
    config.will.topic = fmt::format(mqtt_topic::ROBOT_WILL, m_robotId);
    config.will.payload = R"({"status":"offline"})";
    if(!m_mqtt->init(config))
    {
        LOG_ERROR("mqtt init failed");
        return false;
    }

    registerHandler();
    setSubscribe();

    m_mqtt->setMessageCallback(
        [this](const std::string& topic, const std::string& msg)
        {
            m_router->onMessageProducer(topic, msg);
        });

    if(!m_mqtt->connect())
    {
        LOG_ERROR("mqtt connect failed");
        return false;
    }

    return true;
}

void EdgeCommunication::setSubscribe()
{
    // edge server 状态
    m_mqtt->setSubscribe(mqtt_topic::EDGE_SERVER_STATUS, 1);

    // 注册回复
    {
        std::string topic = mqtt_topic::ROBOT_REGISTER_RSP_PREFIX + m_robotId;
        m_mqtt->setSubscribe(topic, 1);
    }
   
    // 任务接收


//     // AMR 状态上报 (包含心跳功能)
//     std::string sAMRStatusTopic = fmt::format(mqtt_topic::ROBOT_STATUS, "+");
//     m_mqtt->setSubscribe(sAMRStatusTopic, 1);
}

void EdgeCommunication::registerHandler()
{
    m_router->registerHandler(
        mqtt_topic::EDGE_SERVER_STATUS, 
        [this](const std::string& msg)
        {
            //robotRegisterReqHandler(msg);
        });
    
    {
        std::string topic = mqtt_topic::ROBOT_REGISTER_RSP_PREFIX + m_robotId;
        m_router->registerHandler(
        topic,
        [this](const std::string& msg)
        {
            //robotStatusHandler(msg);
        });
    }
    

//     std::string sAMRWillTopic = fmt::format(mqtt_topic::ROBOT_WILL, "+");
//     m_router->registerHandler(
//         sAMRWillTopic, [this](const std::string& msg)
//         {
//             robotWillHandler(msg);
//         });

//       // m_edge_amr_mqtt_msg_router->registerHandler(
//     //     mqtt_topic::MAP_REQUEST,
//     //     [this](const std::string& msg)
//     //     {
//     //         //m_mapService->mapDataReqHandler(msg);
//     //     });

//     // m_edge_amr_mqtt_msg_router->registerHandler(
//     //     mqtt_topic::TRAFFIC_RIGHTS_REQ,
//     //     [this](const std::string& msg)
//     //     {
//     //         //m_trafficService->robotRightHandler(msg);
//     //     });
}

// void EdgeCommunication::robotRegisterReqHandler(const std::string& msg)
// {
//     RobotRegisterRequest req;
//     RobotRegisterResponse resp;
//     bool bRet = MessageCodec::decodeRegisterReq(msg, req);
//     if(!bRet)
//     {
//         LOG_ERROR("decodeRegisterReq failed");
//         return;
//     }

//     bRet = m_context->getRobotService()->handleRegister(req, resp);
//     if(!bRet)
//     {
//         LOG_ERROR("robot register failed");
//     }

//     std::string payload = MessageCodec::encodeRegisterResp(resp);
//     std::string topic = mqtt_topic::ROBOT_REGISTER_RSP_PREFIX + req.robot_id;
//     m_mqtt->publish(topic, payload);
// }

// void EdgeCommunication::robotStatusHandler(const std::string& msg)
// {
//     RobotRunningStatus status;
//     bool bRet =  MessageCodec::decodeRobotRunningStatus(msg, status);
//     if(!bRet)
//     {
//         LOG_ERROR("robotStatusHandler failed");
//         return;
//     }

//     m_context->getRobotService()->handleStatus(status);
// }

// void EdgeCommunication::robotWillHandler(const std::string& msg)
// {
//     RobotRegWillPush push;
//     bool bRet = MessageCodec::decodeRobotWillPush(msg, push);
//     if(!bRet)
//     {
//         LOG_ERROR("robotWillHandler failed");
//         return;
//     }

//     m_context->getRobotService()->handleWill(push.robot_id);
// }

}