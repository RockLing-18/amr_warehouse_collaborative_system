#include "simulation_manager/controller_checker.h"

namespace simulation_manager
{
ControllerChecker::ControllerChecker(const rclcpp::Node::SharedPtr& node) 
: m_node(node)
{
}

bool ControllerChecker::check(const std::string& robot_id, std::chrono::milliseconds timeout)
{
   const std::string service_name = "/" + robot_id + "/controller_manager/list_controllers";

    auto client = m_node->create_client< controller_manager_msgs::srv::ListControllers>(service_name);
    const auto start_time = std::chrono::steady_clock::now();

    // 等待 controller_manager service 出现
    while(rclcpp::ok())
    {
        if(client->wait_for_service(std::chrono::milliseconds(200)))
        {
            break;
        }

        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time);

        if(elapsed >= timeout)
        {
            RCLCPP_WARN(
                m_node->get_logger(),
                "Wait controller_manager service timeout, robot_id=%s",
                robot_id.c_str());

            return false;
        }
    }

    // service 已经存在，开始检查 controller
    while(rclcpp::ok())
    {
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time);

        if(elapsed >= timeout)
        {
            RCLCPP_WARN(
                m_node->get_logger(),
                "Wait controller ready timeout, robot_id=%s",
                robot_id.c_str());

            return false;
        }

        auto request =
            std::make_shared<
                controller_manager_msgs::srv::ListControllers::Request>();

        auto future = client->async_send_request(request);

        auto remaining = timeout - elapsed;

        auto wait_time =
            std::min(
                std::chrono::milliseconds(200),
                remaining);

        auto result =
            rclcpp::spin_until_future_complete(
                m_node,
                future,
                wait_time);

        if(result ==
           rclcpp::FutureReturnCode::SUCCESS)
        {
            auto response = future.get();

            if(isControllerReady(response))
            {
                RCLCPP_INFO(
                    m_node->get_logger(),
                    "Controller ready, robot_id=%s",
                    robot_id.c_str());

                return true;
            }

            RCLCPP_DEBUG(
                m_node->get_logger(),
                "Controller not ready yet, robot_id=%s",
                robot_id.c_str());
        }
        else if(result ==
                rclcpp::FutureReturnCode::TIMEOUT)
        {
            RCLCPP_DEBUG(
                m_node->get_logger(),
                "ListControllers request timeout, robot_id=%s",
                robot_id.c_str());
        }
        else
        {
            RCLCPP_WARN(
                m_node->get_logger(),
                "ListControllers request failed, robot_id=%s",
                robot_id.c_str());
        }

        // 等待一小段时间后再次检查
        std::this_thread::sleep_for(
            std::chrono::milliseconds(200));
    }

    return false; 
}

void ControllerChecker::checkAsync(const std::string& robot_id, Callback callback)
{
    const std::string service = "/" + robot_id + "/controller_manager/list_controllers";

    RCLCPP_INFO(
    m_node->get_logger(),
    "ControllerChecker checkAsync robot=%s service=%s",
    robot_id.c_str(),
    service.c_str());

    auto client = m_node->create_client<controller_manager_msgs::srv::ListControllers>(service);
    if(!client->wait_for_service(std::chrono::seconds(1)))
    {
        RCLCPP_ERROR(
        m_node->get_logger(),
        "Controller service unavailable robot=%s service=%s",
        robot_id.c_str(),
        service.c_str());
        callback(false);
        return;
    }

    
    RCLCPP_INFO(
    m_node->get_logger(),
    "Controller service available robot=%s",
    robot_id.c_str());

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
                RCLCPP_INFO(
                m_node->get_logger(),
                "ControllerChecker response callback robot=%s",
                robot_id.c_str());

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