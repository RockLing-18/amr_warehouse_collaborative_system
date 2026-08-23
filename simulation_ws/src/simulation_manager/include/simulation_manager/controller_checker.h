#pragma once

#include <rclcpp/rclcpp.hpp>
#include <controller_manager_msgs/srv/list_controllers.hpp>

#include <functional>
#include <memory>
#include <string>


namespace simulation_manager
{

class ControllerChecker
{
public:
    using Callback =std::function<void(bool ready)>;

public:
    explicit ControllerChecker(const rclcpp::Node::SharedPtr& node);

    void checkAsync(const std::string& robot_id, Callback callback);

private:
    bool isControllerReady(const controller_manager_msgs::srv::ListControllers::Response::SharedPtr& response);

private:
    rclcpp::Node::SharedPtr m_node;
};


}