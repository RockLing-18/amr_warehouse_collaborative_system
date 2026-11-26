#include "bridge/traffic_bridge.h"

#include "nlohmann/json.hpp"
#include "amr_common/ros_interface_names.h"
#include "mqtt/mqtt_topic.h"
#include "communication/edge_communication.h"
#include "utils/ros_logger.h"

using json = nlohmann::json;

namespace amr_agent
{

TrafficBridge::TrafficBridge(
    rclcpp::Node::SharedPtr node,
    EdgeCommunication* communication,
    const std::string& robot_id)
    : m_node(node),
      m_communication(communication),
      m_robot_id(robot_id)
{
}

void TrafficBridge::init()
{
    m_traffic_request_sub = m_node->create_subscription<TrafficRequest>(
        amr_common::topic::TRAFFIC_REQUEST,
        10,
        [this](const TrafficRequest::SharedPtr request)
        {
            onInternalTrafficRequest(request);
        });

    m_traffic_response_pub = m_node->create_publisher<TrafficResponse>(
        amr_common::topic::TRAFFIC_RESPONSE, 10);

    m_communication->registerExternalHandler(
        mqtt_topic::makeAmrTrafficResponseTopic(m_robot_id),
        [this](const std::string& payload)
        {
            onMqttTrafficResponse(payload);
        });

    m_communication->setExternalSubscribe(
        mqtt_topic::makeAmrTrafficResponseTopic(m_robot_id), 1);

    LOG_INFO("TrafficBridge init done robot=%s", m_robot_id.c_str());
}

void TrafficBridge::onInternalTrafficRequest(const TrafficRequest::SharedPtr request)
{
    if(!request)
        return;

    json payload;
    payload["op"] = request->op;
    payload["request_id"] = request->request_id;
    payload["robot_id"] = request->robot_id;
    payload["resource_id"] = request->resource_id;
    payload["epoch"] = request->epoch;
    payload["eta_sec"] = request->eta_sec;
    payload["lease_ttl_sec"] = request->lease_ttl_sec;

    m_communication->publish(
        mqtt_topic::makeAmrTrafficRequestTopic(m_robot_id),
        payload.dump());
}

void TrafficBridge::onMqttTrafficResponse(const std::string& payload)
{
    try
    {
        auto root = json::parse(payload);

        TrafficResponse response;
        response.request_id = root.value("request_id", "");
        response.robot_id = root.value("robot_id", "");
        response.resource_id = root.value("resource_id", "");
        response.epoch = root.value("epoch", 0);
        response.result = root.value("result", "");
        response.queue_seq = root.value("queue_seq", -1);
        response.lease_expires_at = root.value("lease_expires_at", 0.0);
        response.reason = root.value("reason", "");

        if(response.robot_id != m_robot_id)
        {
            LOG_WARN("drop traffic response robot mismatch expected=%s got=%s",
                m_robot_id.c_str(), response.robot_id.c_str());
            return;
        }

        m_traffic_response_pub->publish(response);
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("parse traffic response failed:%s", e.what());
    }
}

}
