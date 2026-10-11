#pragma once

#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "amr_interfaces/msg/task_command.hpp"
#include "amr_interfaces/msg/task_event.hpp"

namespace amr_agent
{

class EdgeCommunication;

class TaskBridge
{
public:
    using TaskCommand = amr_interfaces::msg::TaskCommand;
    using TaskEvent = amr_interfaces::msg::TaskEvent;

    TaskBridge(
        rclcpp::Node::SharedPtr node,
        EdgeCommunication* communication,
        const std::string& robot_id,
        const std::string& warehouse_id);

    void init();

private:
    void onAmrTaskMessage(const std::string& payload);
    void onTaskStateMessage(const std::string& payload);
    void onGoodsResponseMessage(const std::string& payload);
    void onStatusAckMessage(const std::string& payload);
    void onInternalTaskEvent(const TaskEvent::SharedPtr event);

    void publishTaskEventToMqtt(const TaskEvent& event);

    std::string makeMessageId(const std::string& event_type) const;
    std::string nowIso8601() const;

private:
    rclcpp::Node::SharedPtr m_node;
    EdgeCommunication* m_communication;
    std::string m_robot_id;
    std::string m_warehouse_id;

    rclcpp::Publisher<TaskCommand>::SharedPtr m_task_command_pub;
    rclcpp::Subscription<TaskEvent>::SharedPtr m_task_event_sub;
};

}
