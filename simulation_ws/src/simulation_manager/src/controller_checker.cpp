#include "simulation_manager/controller_checker.h"
#include <future>
#include <thread>
#include <chrono>

namespace simulation_manager
{
ControllerChecker::ControllerChecker(const rclcpp::Node::SharedPtr& node) 
: m_node(node), m_logger(node->get_logger())
{
}

bool ControllerChecker::check(const std::string& robot_id, std::chrono::milliseconds timeout)
{
    const std::string service_name =
        "/" + robot_id + "/controller_manager/list_controllers";

    RCLCPP_INFO(
        m_logger,
        "Check controller robot=%s service=%s",
        robot_id.c_str(),
        service_name.c_str());

    auto node = m_node.lock();
    if(!node)
        return false;
    
    auto client =
        node->create_client<
            controller_manager_msgs::srv::ListControllers>(
                service_name);

    const auto start_time =
        std::chrono::steady_clock::now();

    /*
     * 1. 等待 controller_manager service
     */
    while(rclcpp::ok())
    {
        if(client->wait_for_service(
               std::chrono::milliseconds(200)))
        {
            RCLCPP_INFO(
                m_logger,
                "Controller service available robot=%s",
                robot_id.c_str());

            break;
        }

        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time);

        if(elapsed >= timeout)
        {
            RCLCPP_ERROR(
                m_logger,
                "Controller service timeout robot=%s service=%s",
                robot_id.c_str(),
                service_name.c_str());

            return false;
        }
    }

    /*
     * 2. 循环检查 controller 状态
     */
    while(rclcpp::ok())
    {
        const auto now =
            std::chrono::steady_clock::now();

        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - start_time);

        if(elapsed >= timeout)
        {
            RCLCPP_WARN(
                m_logger,
                "Controller ready timeout robot=%s",
                robot_id.c_str());

            return false;
        }

        /*
         * 创建请求
         */
        auto request =
            std::make_shared<
                controller_manager_msgs::srv::ListControllers::Request>();

        /*
         * 使用 promise 保存异步结果
         */
        auto promise =
            std::make_shared<
                std::promise<
                    controller_manager_msgs::srv::ListControllers::Response::SharedPtr>>();

        auto future = promise->get_future();

        /*
         * 发送异步请求
         *
         * 注意：
         * 这里绝对不要 spin_until_future_complete()
         */
        client->async_send_request(
            request,
            [promise](
                rclcpp::Client<
                    controller_manager_msgs::srv::ListControllers>::SharedFuture future)
            {
                try
                {
                    promise->set_value(future.get());
                }
                catch(...)
                {
                    try
                    {
                        promise->set_exception(
                            std::current_exception());
                    }
                    catch(...)
                    {
                    }
                }
            });

        /*
         * 等待 response
         *
         * ROS2 executor 会负责执行上面的 callback。
         *
         * 当前线程只是等待 future，
         * 不负责 spin ROS2。
         */
        const auto result =
            future.wait_for(
                std::chrono::milliseconds(200));

        if(result == std::future_status::ready)
        {
            try
            {
                auto response = future.get();

                if(isControllerReady(response))
                {
                    RCLCPP_INFO(
                        m_logger,
                        "Controller ready robot=%s",
                        robot_id.c_str());

                    return true;
                }

                RCLCPP_DEBUG(
                    m_logger,
                    "Controller not ready yet robot=%s",
                    robot_id.c_str());
            }
            catch(const std::exception& e)
            {
                RCLCPP_WARN(
                    m_logger,
                    "Controller check response failed "
                    "robot=%s error=%s",
                    robot_id.c_str(),
                    e.what());
            }
        }
        else
        {
            RCLCPP_DEBUG(
                m_logger,
                "ListControllers response timeout robot=%s",
                robot_id.c_str());
        }

        /*
         * 3. 200ms 后重新检查
         */
        std::this_thread::sleep_for(
            std::chrono::milliseconds(200));
    }

    return false; 
}

void ControllerChecker::checkAsync(const std::string& robot_id, Callback callback)
{
    const std::string service = "/" + robot_id + "/controller_manager/list_controllers";

    RCLCPP_INFO(
    m_logger,
    "ControllerChecker checkAsync robot=%s service=%s",
    robot_id.c_str(),
    service.c_str());

    auto node = m_node.lock();
    if(!node)
        return;
    
    auto client = node->create_client<controller_manager_msgs::srv::ListControllers>(service);
    if(!client->wait_for_service(std::chrono::seconds(1)))
    {
        RCLCPP_ERROR(
        m_logger,
        "Controller service unavailable robot=%s service=%s",
        robot_id.c_str(),
        service.c_str());
        callback(false);
        return;
    }

    
    RCLCPP_INFO(
    m_logger,
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
                m_logger,
                "ControllerChecker response callback robot=%s",
                robot_id.c_str());

                auto response = future.get();
                bool ready = isControllerReady(response);
                callback(ready);
            }
            catch(const std::exception& e)
            {
                RCLCPP_ERROR(
                    m_logger,
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
            m_logger,
            "controller=%s state=%s",
            controller.name.c_str(),
            controller.state.c_str());

        if(controller.name == "amr_diff_drive_controller" && controller.state == "active")
            return true;
    }

    return false;
}

}