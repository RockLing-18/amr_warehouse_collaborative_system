#include "simulation_manager/gazebo_client.h"
#include <cstring>

namespace simulation_manager
{
GazeboClient::GazeboClient(const rclcpp::Node::SharedPtr& node) : m_node(node)
{
    // m_get_world_properties_client = m_node->create_client<gazebo_msgs::srv::GetWorldProperties>("/gazebo/get_world_properties");
    m_get_model_list_client = m_node->create_client<gazebo_msgs::srv::GetModelList>("/get_model_list");
    m_delete_entity_client = m_node->create_client<gazebo_msgs::srv::DeleteEntity>("/delete_entity");
}

void GazeboClient::getModelsAsync(GetModelsCallback callback)
{
    if (!m_get_model_list_client->service_is_ready())
    {
        RCLCPP_WARN( m_node->get_logger(), "Gazebo get_model_list service is not ready");
        callback({});
        return;
    }

    auto request = std::make_shared<gazebo_msgs::srv::GetModelList::Request>();
    m_get_model_list_client->async_send_request(
        request,
        [this, callback](rclcpp::Client<gazebo_msgs::srv::GetModelList>::SharedFuture future)
        {
            try
            {
                const auto response = future.get();
                if (!response->success)
                {
                    RCLCPP_ERROR(m_node->get_logger(), "Gazebo get_model_list failed");
                    callback({});
                    return;
                }

                std::vector<GazeboModelInfo> models;
                for (const auto& name : response->model_names)
                {
                    RCLCPP_INFO(m_node->get_logger(),"Gazebo model: %s", name.c_str());
                    GazeboModelInfo info;
                    if (parseModelName(name, info))
                        models.push_back(info);
                }

                callback(models);
            }
            catch (const std::exception& e)
            {
                RCLCPP_ERROR(m_node->get_logger(), "Exception while getting Gazebo models: %s", e.what());
            }
        });
}

void GazeboClient::deleteModelAsync(const std::string& model_name, std::function<void(bool)> callback)
{
    if (!m_delete_entity_client->service_is_ready())
    {
        RCLCPP_WARN(m_node->get_logger(), "Gazebo delete_entity service is not ready");
        callback(false);
        return;
    }

    auto request =std::make_shared<gazebo_msgs::srv::DeleteEntity::Request>();
    request->name = model_name;
    m_delete_entity_client->async_send_request(
        request,
        [this, model_name, callback](rclcpp::Client<gazebo_msgs::srv::DeleteEntity>::SharedFuture future)
        {
            try
            {
                const auto response = future.get();
                if (!response->success)
                {
                    RCLCPP_ERROR( m_node->get_logger(),"Gazebo failed to delete model: %s, status=%s", model_name.c_str(), response->status_message.c_str());
                    callback(false);
                    return;
                }

                RCLCPP_INFO(m_node->get_logger(),"Gazebo model deleted: %s", model_name.c_str());
                callback(true);
            }
            catch (const std::exception& e)
            {
                RCLCPP_ERROR(m_node->get_logger(), "Exception while deleting Gazebo model %s: %s", model_name.c_str(), e.what());
                callback(false);
            }
        });
}

// bool GazeboClient::getModels(std::vector<GazeboModelInfo>& models)
// {
//     if (!m_get_model_list_client->wait_for_service(std::chrono::seconds(1)))
//     {
//         RCLCPP_WARN(m_node->get_logger(), "Gazebo m_get_model_list service unavailable");
//         return false;
//     }

//     auto request = std::make_shared<gazebo_msgs::srv::GetModelList::Request>();
//     auto future = m_get_model_list_client->async_send_request(request);

//     if (rclcpp::spin_until_future_complete(m_node, future, std::chrono::seconds(2)) != rclcpp::FutureReturnCode::SUCCESS)
//     {
//         RCLCPP_ERROR(m_node->get_logger(), "Failed to get Gazebo world properties");
//         return false;
//     }
//     // if (future.wait_for(std::chrono::seconds(2)) != std::future_status::ready)
//     // {
//     //     RCLCPP_ERROR(m_node->get_logger(), "Failed to get Gazebo model list");
//     //     return false;
//     // }

//     auto response = future.get();
//     models.clear();

//     for (const auto& name : response->model_names)
//     {
//         GazeboModelInfo info;
//         RCLCPP_INFO(m_node->get_logger(), "Gazebo world model_names :%s", name.c_str()); 
//         if(parseModelName(name, info))
//             models.push_back(info);
//     }

//     return true;
// }

// bool GazeboClient::deleteModel(const std::string& model_name)
// {
//     if (!m_delete_entity_client->wait_for_service(std::chrono::seconds(1)))
//     {
//         RCLCPP_WARN(m_node->get_logger(), "Gazebo delete_entity service unavailable");
//         return false;
//     }

//     auto request = std::make_shared<gazebo_msgs::srv::DeleteEntity::Request>();
//     request->name = model_name;

//     auto future = m_delete_entity_client->async_send_request(request);

//     if (rclcpp::spin_until_future_complete(m_node, future, std::chrono::seconds(2)) != rclcpp::FutureReturnCode::SUCCESS)
//     {
//         RCLCPP_ERROR(m_node->get_logger(), "Failed to delete Gazebo model: %s", model_name.c_str());
//         return false;
//     }
//     // if (future.wait_for(std::chrono::seconds(2)) != std::future_status::ready)
//     // {
//     //     RCLCPP_ERROR( m_node->get_logger(), "Failed to delete Gazebo model: %s", model_name.c_str());
//     //     return false;
//     // }

//     auto response = future.get();
//     if (!response->success)
//     {
//         RCLCPP_ERROR(
//             m_node->get_logger(),
//             "Gazebo failed to delete model: %s, status=%s",
//             model_name.c_str(),
//             response->status_message.c_str());

//         return false;
//     }

//     RCLCPP_INFO(m_node->get_logger(), "Gazebo model deleted: %s", model_name.c_str());
//     return true;
// }

bool GazeboClient::parseModelName(const std::string& model_name, GazeboModelInfo& info)
{
    constexpr const char* prefix = "amr_";
    if (model_name.rfind(prefix, 0) != 0)
        return false;
    
    const std::string value = model_name.substr(std::strlen(prefix));
    const auto pos = value.find('_');

    if (pos == std::string::npos || pos == 0 || pos == value.size() - 1)
        return false;

    info.model_name = model_name;
    info.robot_id = value.substr(0, pos);
    info.instance_id = value.substr(pos + 1);

    return true;
}

}