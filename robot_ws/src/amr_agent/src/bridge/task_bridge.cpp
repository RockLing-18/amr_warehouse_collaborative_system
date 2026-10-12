#include "bridge/task_bridge.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "nlohmann/json.hpp"
#include "amr_common/ros_interface_names.h"
#include "mqtt/mqtt_topic.h"
#include "communication/edge_communication.h"
#include "utils/ros_logger.h"

using json = nlohmann::json;

namespace amr_agent
{

TaskBridge::TaskBridge(
    rclcpp::Node::SharedPtr node,
    EdgeCommunication* communication,
    const std::string& robot_id,
    const std::string& warehouse_id,
    const std::string& pending_report_path)
    : m_node(node),
      m_communication(communication),
      m_robot_id(robot_id),
      m_warehouse_id(warehouse_id),
      m_pending_reports(pending_report_path)
{
}

void TaskBridge::init()
{
    m_task_command_pub = m_node->create_publisher<TaskCommand>(
        amr_common::topic::TASK_COMMAND, 10);

    m_task_event_sub = m_node->create_subscription<TaskEvent>(
        amr_common::topic::TASK_EVENT,
        10,
        [this](const TaskEvent::SharedPtr event)
        {
            onInternalTaskEvent(event);
        });

    m_communication->registerExternalHandler(
        mqtt_topic::makeAmrTaskTopic(m_robot_id),
        [this](const std::string& payload)
        {
            onAmrTaskMessage(payload);
        });

    m_communication->registerExternalHandler(
        mqtt_topic::makeWarehouseTaskStateTopic(m_warehouse_id, "+"),
        [this](const std::string& payload)
        {
            onTaskStateMessage(payload);
        });

    m_communication->registerExternalHandler(
        mqtt_topic::makeAmrTaskGoodsResponseTopic(m_robot_id),
        [this](const std::string& payload)
        {
            onGoodsResponseMessage(payload);
        });

    m_communication->registerExternalHandler(
        mqtt_topic::makeAmrTaskStatusAckTopic(m_robot_id),
        [this](const std::string& payload)
        {
            onStatusAckMessage(payload);
        });

    m_communication->setExternalSubscribe(
        mqtt_topic::makeAmrTaskTopic(m_robot_id), 1);
    m_communication->setExternalSubscribe(
        mqtt_topic::makeWarehouseTaskStateTopic(m_warehouse_id, "+"), 1);
    m_communication->setExternalSubscribe(
        mqtt_topic::makeAmrTaskGoodsResponseTopic(m_robot_id), 1);
    m_communication->setExternalSubscribe(
        mqtt_topic::makeAmrTaskStatusAckTopic(m_robot_id), 1);

    m_pending_flush_timer = m_node->create_wall_timer(
        std::chrono::seconds(5),
        [this]()
        {
            flushPendingReports();
        });

    LOG_INFO("TaskBridge init done robot=%s warehouse=%s", m_robot_id.c_str(), m_warehouse_id.c_str());
}

void TaskBridge::onAmrTaskMessage(const std::string& payload)
{
    try
    {
        auto root = json::parse(payload);
        TaskCommand cmd;
        cmd.command_type = root.value("command_type", "TASK_ASSIGN");
        cmd.task_id = root.value("task_id", "");
        cmd.warehouse_id = root.value("warehouse_id", m_warehouse_id);
        cmd.task_type = root.value("task_type", "");
        cmd.state = root.value("state", "");
        cmd.reason = root.value("reason", "");
        cmd.stamp = m_node->now();

        if(root.contains("robot_ids") && root["robot_ids"].is_array())
        {
            for(const auto& robot : root["robot_ids"])
                cmd.robot_ids.push_back(robot.get<std::string>());
        }

        if(root.contains("goods") && root["goods"].is_array())
        {
            for(const auto& goodsJson : root["goods"])
            {
                amr_interfaces::msg::TaskGoods goods;
                goods.goods_id = goodsJson.value("goods_id", "");
                goods.goods_name = goodsJson.value("goods_name", "");
                goods.total_quantity = goodsJson.value("quantity", 0);
                cmd.goods.push_back(goods);
            }
        }

        m_task_command_pub->publish(cmd);
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("parse amr task failed:%s", e.what());
    }
}

void TaskBridge::onTaskStateMessage(const std::string& payload)
{
    try
    {
        auto root = json::parse(payload);
        TaskCommand cmd;
        cmd.command_type = "TASK_STATE_UPDATE";
        cmd.task_id = root.value("task_id", "");
        cmd.warehouse_id = root.value("warehouse_id", "");
        cmd.state = root.value("state", "");
        cmd.stamp = m_node->now();

        if(root.contains("goods_status") && root["goods_status"].is_array())
        {
            for(const auto& statusJson : root["goods_status"])
            {
                amr_interfaces::msg::TaskGoodsStatus status;
                status.goods_id = statusJson.value("goods_id", "");
                status.goods_name = statusJson.value("goods_name", "");
                status.total_quantity = statusJson.value("total_quantity", 0);
                status.in_progress_quantity = statusJson.value("in_progress_quantity", 0);
                status.completed_quantity = statusJson.value("completed_quantity", 0);
                status.pending_quantity = statusJson.value("pending_quantity", 0);
                cmd.goods_status.push_back(status);
            }
        }

        m_task_command_pub->publish(cmd);
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("parse task state failed:%s", e.what());
    }
}

void TaskBridge::onGoodsResponseMessage(const std::string& payload)
{
    try
    {
        auto root = json::parse(payload);
        TaskCommand cmd;
        cmd.command_type = "GOODS_RESPONSE";
        cmd.task_id = root.value("task_id", "");
        cmd.warehouse_id = root.value("warehouse_id", "");
        cmd.request_id = root.value("request_id", "");
        cmd.accepted = root.value("accepted", false);
        cmd.reason = root.value("reason", "");
        cmd.stamp = m_node->now();

        if(root.contains("goods") && root["goods"].is_array())
        {
            for(const auto& goodsJson : root["goods"])
            {
                amr_interfaces::msg::TaskGoods goods;
                goods.goods_id = goodsJson.value("goods_id", "");
                goods.goods_name = goodsJson.value("goods_name", "");
                goods.total_quantity = goodsJson.value("quantity", 0);
                cmd.goods.push_back(goods);
            }
        }

        m_task_command_pub->publish(cmd);
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("parse goods response failed:%s", e.what());
    }
}

void TaskBridge::onStatusAckMessage(const std::string& payload)
{
    try
    {
        auto root = json::parse(payload);
        std::vector<std::string> acked;
        if(root.contains("acked_message_ids") && root["acked_message_ids"].is_array())
        {
            for(const auto& id : root["acked_message_ids"])
                acked.push_back(id.get<std::string>());
        }

        m_pending_reports.removeByMessageIds(acked);
        LOG_INFO(
            "receive task status ack robot=%s task=%s count=%zu",
            m_robot_id.c_str(),
            root.value("task_id", "").c_str(),
            acked.size());
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("parse status ack failed:%s", e.what());
    }
}

void TaskBridge::flushPendingReports()
{
    if(!m_communication || !m_communication->isConnected())
        return;

    for(const auto& report : m_pending_reports.load())
    {
        if(m_communication->publish(report.topic, report.payload))
        {
            LOG_INFO("flush pending report message_id=%s", report.message_id.c_str());
        }
    }
}

void TaskBridge::onInternalTaskEvent(const TaskEvent::SharedPtr event)
{
    if(!event)
        return;

    publishTaskEventToMqtt(*event);
}

void TaskBridge::publishTaskEventToMqtt(const TaskEvent& event)
{
    auto [topic, payload] = buildMqttMessage(event);
    if(topic.empty())
        return;

    if(m_communication->isConnected())
    {
        if(!m_communication->publish(topic, payload))
        {
            PendingReport report;
            report.message_id = json::parse(payload).value("message_id", "");
            report.topic = topic;
            report.payload = payload;
            m_pending_reports.add(report);
        }
    }
    else
    {
        PendingReport report;
        report.message_id = json::parse(payload).value("message_id", "");
        report.topic = topic;
        report.payload = payload;
        m_pending_reports.add(report);
    }
}

std::pair<std::string, std::string> TaskBridge::buildMqttMessage(const TaskEvent& event) const
{
    json root;
    root["message_id"] = makeMessageId(event.event_type);
    root["warehouse_id"] = event.warehouse_id.empty() ? m_warehouse_id : event.warehouse_id;
    root["task_id"] = event.task_id;
    root["robot_id"] = m_robot_id;

    if(event.event_type == "GOODS_REQUEST")
    {
        root["request_id"] = event.request_id;
        root["goods"] = json::array();
        for(const auto& goods : event.goods)
        {
            json g;
            g["goods_id"] = goods.goods_id;
            g["goods_name"] = goods.goods_name;
            g["quantity"] = goods.total_quantity;
            root["goods"].push_back(g);
        }

        return {mqtt_topic::makeAmrTaskGoodsRequestTopic(m_robot_id), root.dump()};
    }

    root["event_type"] = event.event_type;
    root["task_state"] = event.task_state;
    root["reason"] = event.reason;
    root["request_id"] = event.request_id;
    root["timestamp"] = nowIso8601();
    root["goods"] = json::array();
    for(const auto& goods : event.goods)
    {
        json g;
        g["goods_id"] = goods.goods_id;
        g["goods_name"] = goods.goods_name;
        g["quantity"] = goods.total_quantity;
        root["goods"].push_back(g);
    }

    return {mqtt_topic::makeAmrTaskStatusTopic(m_robot_id), root.dump()};
}

std::string TaskBridge::makeMessageId(const std::string& event_type) const
{
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return m_robot_id + "_" + std::to_string(ms) + "_" + event_type;
}

std::string TaskBridge::nowIso8601() const
{
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&tt, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S%z");
    return oss.str();
}

}
