#pragma once

#include "simulation_manager/data_define.h"
#include <rclcpp/rclcpp.hpp>
// #include <gazebo_msgs/srv/get_world_properties.hpp>
#include <gazebo_msgs/srv/get_model_list.hpp>
#include <gazebo_msgs/srv/delete_entity.hpp>


namespace simulation_manager
{

class GazeboClient
{
public:
    explicit GazeboClient(const rclcpp::Node::SharedPtr& node);

    using GetModelsCallback = std::function<void(const std::vector<GazeboModelInfo>&)>;

    void getModelsAsync(GetModelsCallback callback);

    void deleteModelAsync(const std::string& model_name, std::function<void(bool)> callback);

    // bool getModels(std::vector<GazeboModelInfo>& models);
    // bool deleteModel(const std::string& model_name);

private:
    bool parseModelName(const std::string& model_name, GazeboModelInfo& info);
private:
    rclcpp::Node::SharedPtr m_node;
    // rclcpp::Client<gazebo_msgs::srv::GetWorldProperties>::SharedPtr m_get_world_properties_client;
    rclcpp::Client<gazebo_msgs::srv::GetModelList>::SharedPtr m_get_model_list_client;
    rclcpp::Client<gazebo_msgs::srv::DeleteEntity>::SharedPtr m_delete_entity_client;
};

}