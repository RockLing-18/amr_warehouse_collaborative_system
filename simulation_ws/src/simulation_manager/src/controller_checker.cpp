#include "simulation_manager/controller_checker.h"

namespace simulation_manager
{
ControllerChecker::ControllerChecker(const rclcpp::Node::SharedPtr& node) 
: m_node(node)
{
}

void ControllerChecker::checkAsync(const std::string& robot_id, Callback callback)
{
    const std::string service = "/" + robot_id + "/controller_manager/list_controllers";
    auto client = m_node->create_client<controller_manager_msgs::srv::ListControllers>(service);
    if(!client->wait_for_service(std::chrono::seconds(1)))
    {
        callback(false);
        return;
    }

    auto request = std::make_shared<controller_manager_msgs::srv::ListControllers::Request>();
    client->async_send_request(
        request,
        [this,
         robot_id,
         client,
         callback]
        (
            rclcpp::Client<controller_manager_msgs::srv::ListControllers>::SharedFuture future
        )
        {
            try
            {
                auto response = future.get();
                bool ready = isControllerReady(response);
                callback(ready);
            }
            catch(const std::exception& e)
            {
                RCLCPP_ERROR(
                    m_node->get_logger(),
                    "Controller check failed robot=%s error=%s",
                    robot_id.c_str(),
                    e.what());

                callback(false);
            }
        });
}


bool ControllerChecker::isControllerReady(const controller_manager_msgs::srv::ListControllers::Response::SharedPtr& response)
{
    for(const auto& controller : response->controller)
    {
        RCLCPP_DEBUG(
            m_node->get_logger(),
            "controller=%s state=%s",
            controller.name.c_str(),
            controller.state.c_str());

        if(controller.name == "amr_diff_drive_controller" && controller.state == "active")
            return true;
    }

    return false;
}

}