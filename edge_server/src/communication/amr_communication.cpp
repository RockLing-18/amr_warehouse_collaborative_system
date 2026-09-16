#include "communication/amr_communication.h"
#include "utils/LogDefine.h"
#include "mqtt/mqtt_client.h"
#include "mqtt/mqtt_topic.h"
#include "mqtt/mqtt_message_router.h"
#include "context/service_context.h"
#include "service/robot_service.h"
#include "message/message_codec.h"

namespace edge_server
{
AmrCommunication::AmrCommunication()
{
    m_mqtt = std::make_unique<MqttClient>();
    m_router =  std::make_unique<MqttMessageRouter>();
}

bool AmrCommunication::init(const MqttCfg& cfg, const std::shared_ptr<ServiceContext>& serviceContext)
{
    m_context = serviceContext;

    if(!m_mqtt->init(cfg))
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

void AmrCommunication::setSubscribe()
{
    // AMR 注册 edge server
    m_mqtt->setSubscribe(mqtt_topic::ROBOT_REGISTER_REQ, 1);

    // AMR 状态上报 (包含心跳功能)
    std::string sAMRStatusTopic = fmt::format(mqtt_topic::ROBOT_STATUS, "+");
    m_mqtt->setSubscribe(sAMRStatusTopic, 1);
}

void AmrCommunication::registerHandler()
{
    m_router->registerHandler(
        mqtt_topic::ROBOT_REGISTER_REQ, 
        [this](const std::string& msg)
        {
            robotRegisterReqHandler(msg);
        });
    
    m_router->registerHandler(
        mqtt_topic::ROBOT_STATUS,
        [this](const std::string& msg)
        {
            robotStatusHandler(msg);
        });

    std::string sAMRWillTopic = fmt::format(mqtt_topic::ROBOT_WILL, "+");
    m_router->registerHandler(
        sAMRWillTopic, [this](const std::string& msg)
        {
            robotWillHandler(msg);
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

void AmrCommunication::robotRegisterReqHandler(const std::string& msg)
{
    RobotRegisterRequest req;
    RobotRegisterResponse resp;
    bool bRet = MessageCodec::decodeRegisterReq(msg, req);
    if(!bRet)
    {
        LOG_ERROR("decodeRegisterReq failed");
        return;
    }

    bRet = m_context->getRobotService()->handleRegister(req, resp);
    if(!bRet)
    {
        LOG_ERROR("robot register failed");
    }

    std::string payload = MessageCodec::encodeRegisterResp(resp);
    std::string topic = mqtt_topic::ROBOT_REGISTER_RSP_PREFIX + req.robot_id;
    m_mqtt->publish(topic, payload);
}

void AmrCommunication::robotStatusHandler(const std::string& msg)
{
    RobotRunningStatus status;
    bool bRet =  MessageCodec::decodeRobotRunningStatus(msg, status);
    if(!bRet)
    {
        LOG_ERROR("robotStatusHandler failed");
        return;
    }

    m_context->getRobotService()->handleStatus(status);
}

void AmrCommunication::robotWillHandler(const std::string& msg)
{
    RobotRegWillPush push;
    bool bRet = MessageCodec::decodeRobotWillPush(msg, push);
    if(!bRet)
    {
        LOG_ERROR("robotWillHandler failed");
        return;
    }

    m_context->getRobotService()->handleWill(push.robot_id);
}

}