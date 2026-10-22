#include "communication/amr_communication.h"
#include "utils/LogDefine.h"
#include "mqtt/mqtt_client.h"
#include "mqtt/mqtt_topic.h"
#include "mqtt/mqtt_message_router.h"
#include "context/service_context.h"
#include "service/robot_service.h"
#include "service/task_service.h"
#include "message/message_codec.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

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

    // AMR 离线 Will
    m_mqtt->setSubscribe(fmt::format(mqtt_topic::ROBOT_WILL, "+"), 1);

    m_mqtt->setSubscribe(fmt::format(mqtt_topic::AMR_TASK_GOODS_REQUEST, "+"), 1);
    m_mqtt->setSubscribe(fmt::format(mqtt_topic::AMR_TASK_STATUS, "+"), 1);
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
        fmt::format(mqtt_topic::ROBOT_STATUS, "+"),
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

    m_router->registerHandler(
        fmt::format(mqtt_topic::AMR_TASK_GOODS_REQUEST, "+"),
        [this](const std::string& msg)
        {
            robotGoodsRequestHandler(msg);
        });

    m_router->registerHandler(
        fmt::format(mqtt_topic::AMR_TASK_STATUS, "+"),
        [this](const std::string& msg)
        {
            robotTaskStatusHandler(msg);
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

bool AmrCommunication::publish(const std::string& topic, const std::string& payload, int qos, bool retain)
{
    return m_mqtt->publish(topic, payload, qos, retain);
}

void AmrCommunication::robotGoodsRequestHandler(const std::string& msg)
{
    try
    {
        auto root = json::parse(msg);
        GoodsApplyRequest request;
        request.request_id = root.value("request_id", "");
        request.warehouse_id = root.value("warehouse_id", "");
        request.task_id = root.value("task_id", "");
        request.robot_id = root.value("robot_id", "");
        if(root.contains("goods") && root["goods"].is_array())
        {
            for(const auto& item : root["goods"])
            {
                TaskGoods goods;
                goods.goods_id = item.value("goods_id", "");
                goods.goods_name = item.value("goods_name", "");
                goods.total_quantity = item.value("quantity", 0);
                request.goods.push_back(goods);
            }
        }

        GoodsApplyResult result;
        m_context->getTaskService()->handleGoodsApply(request, result);

        json resp;
        resp["request_id"] = request.request_id;
        resp["warehouse_id"] = request.warehouse_id;
        resp["task_id"] = request.task_id;
        resp["robot_id"] = request.robot_id;
        resp["accepted"] = result.accepted;
        resp["reason"] = result.reason;
        resp["goods"] = json::array();
        for(const auto& goods : result.goods)
        {
            json g;
            g["goods_id"] = goods.goods_id;
            g["goods_name"] = goods.goods_name;
            g["quantity"] = goods.total_quantity;
            g["accepted"] = result.accepted;
            resp["goods"].push_back(g);
        }

        m_mqtt->publish(
            mqtt_topic::makeAmrTaskGoodsResponseTopic(request.robot_id),
            resp.dump());

        std::string statePayload = m_context->getTaskService()->buildTaskStateMessage(request.task_id);
        if(!statePayload.empty())
        {
            m_mqtt->publish(
                mqtt_topic::makeWarehouseTaskStateTopic(request.warehouse_id, request.task_id),
                statePayload);
        }
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("handle goods request exception:%s", e.what());
    }
}

void AmrCommunication::robotTaskStatusHandler(const std::string& msg)
{
    try
    {
        auto root = json::parse(msg);
        AmrTaskStatusEvent event;
        event.message_id = root.value("message_id", "");
        event.warehouse_id = root.value("warehouse_id", "");
        event.task_id = root.value("task_id", "");
        event.robot_id = root.value("robot_id", "");
        event.event_type = root.value("event_type", "");
        event.task_state = root.value("task_state", "");
        event.request_id = root.value("request_id", "");
        event.reason = root.value("reason", "");
        event.timestamp = root.value("timestamp", "");
        event.goods_json = root.contains("goods") ? root["goods"].dump() : "";

        m_context->getTaskService()->handleAmrTaskStatus(event);

        std::string statePayload = m_context->getTaskService()->buildTaskStateMessage(event.task_id);
        if(!statePayload.empty())
        {
            m_mqtt->publish(
                mqtt_topic::makeWarehouseTaskStateTopic(event.warehouse_id, event.task_id),
                statePayload);
        }

        json ack;
        ack["warehouse_id"] = event.warehouse_id;
        ack["task_id"] = event.task_id;
        ack["robot_id"] = event.robot_id;
        ack["acked_message_ids"] = json::array({event.message_id});
        ack["timestamp"] = event.timestamp;
        m_mqtt->publish(
            mqtt_topic::makeAmrTaskStatusAckTopic(event.robot_id),
            ack.dump());
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("handle amr task status exception:%s", e.what());
    }
}

}
