#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <algorithm>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"

#include "amr_behavior_manager/navigation_client.h"
#include "amr_behavior_manager/zone_station_loader.h"
#include "amr_behavior_manager/goods_point_resolver.h"
#include "amr_common/ros_interface_names.h"
#include "amr_interfaces/msg/robot_status.hpp"
#include "amr_interfaces/msg/task_status.hpp"
#include "amr_interfaces/msg/task_command.hpp"
#include "amr_interfaces/msg/task_event.hpp"
#include "amr_interfaces/msg/map_info.hpp"
#include "amr_interfaces/msg/edge_status.hpp"
#include "amr_interfaces/msg/component_status.hpp"

namespace amr_behavior_manager
{

class AmrBehaviorManagerNode : public rclcpp::Node
{
public:
    using RobotStatus = amr_interfaces::msg::RobotStatus;
    using TaskStatus = amr_interfaces::msg::TaskStatus;
    using TaskCommand = amr_interfaces::msg::TaskCommand;
    using TaskEvent = amr_interfaces::msg::TaskEvent;
    using TaskGoods = amr_interfaces::msg::TaskGoods;
    using TaskGoodsStatus = amr_interfaces::msg::TaskGoodsStatus;
    using ComponentStatus = amr_interfaces::msg::ComponentStatus;

    AmrBehaviorManagerNode()
        : Node("amr_behavior_manager")
    {
        this->declare_parameter<std::string>("robot_id", "robot_01");
        this->declare_parameter<double>("status_publish_rate_hz", 1.0);
        this->declare_parameter<double>("task_status_period_s", 5.0);
        this->declare_parameter<double>("grasp_simulate_seconds", 1.0);
        this->declare_parameter<double>("place_simulate_seconds", 1.0);
        this->declare_parameter<int>("carry_capacity", 10);

        m_robot_status_pub = this->create_publisher<RobotStatus>(
            amr_common::topic::ROBOT_STATUS, 10);
        m_task_status_pub = this->create_publisher<TaskStatus>(
            amr_common::topic::TASK_STATUS, 10);
        m_component_status_pub = this->create_publisher<ComponentStatus>(
            amr_common::topic::BEHAVIOR_STATUS, 10);
        m_task_command_sub = this->create_subscription<TaskCommand>(
            amr_common::topic::TASK_COMMAND,
            10,
            [this](const TaskCommand::SharedPtr cmd)
            {
                handleTaskCommand(cmd);
            });
        m_task_event_pub = this->create_publisher<TaskEvent>(
            amr_common::topic::TASK_EVENT, 10);

        auto map_info_qos = rclcpp::QoS(rclcpp::KeepLast(1));
        map_info_qos.reliable();
        map_info_qos.transient_local();
        m_map_info_sub = this->create_subscription<amr_interfaces::msg::MapInfo>(
            amr_common::topic::MAP_INFO,
            map_info_qos,
            [this](const amr_interfaces::msg::MapInfo::SharedPtr msg)
            {
                if(!msg || !msg->ready)
                    return;

                m_zone_station_loader = std::make_shared<ZoneStationLoader>();
                if(m_zone_station_loader->load(msg->zones_yaml, msg->stations_yaml))
                {
                    m_goods_point_resolver = std::make_shared<GoodsPointResolver>(m_zone_station_loader);
                    m_map_ready = true;
                    RCLCPP_INFO(
                        get_logger(),
                        "map info ready and zone/station loaded");
                }
                else
                {
                    m_map_ready = false;
                    RCLCPP_ERROR(
                        get_logger(),
                        "map info ready but zone/station load failed");
                }
            });

        m_edge_status_sub = this->create_subscription<amr_interfaces::msg::EdgeStatus>(
            amr_common::topic::EDGE_STATUS,
            10,
            [this](const amr_interfaces::msg::EdgeStatus::SharedPtr msg)
            {
                if(!msg)
                    return;
                m_edge_online = msg->online;
                RCLCPP_INFO(get_logger(), "edge online=%s", m_edge_online ? "true" : "false");
            });
    }

    void init()
    {
        m_nav_client = std::make_shared<NavigationClient>(shared_from_this());
        m_nav_client->setFeedbackCallback(
            [this](const std::shared_ptr<const NavigationClient::AmrNavigateToPose::Feedback> feedback)
            {
                (void)feedback;
            });

        m_nav_client->setResultCallback(
            [this](const NavigationClient::GoalHandle::WrappedResult& result)
            {
                bool success = result.code == rclcpp_action::ResultCode::SUCCEEDED;
                onNavigationResult(success);
            });

        double robotStatusRate = this->get_parameter("status_publish_rate_hz").as_double();
        m_robot_status_timer = this->create_wall_timer(
            std::chrono::duration<double>(1.0 / robotStatusRate),
            std::bind(&AmrBehaviorManagerNode::publishRobotStatus, this));

        double taskStatusPeriod = this->get_parameter("task_status_period_s").as_double();
        m_task_status_timer = this->create_wall_timer(
            std::chrono::duration<double>(taskStatusPeriod),
            std::bind(&AmrBehaviorManagerNode::publishPeriodicTaskStatus, this));

        RCLCPP_INFO(this->get_logger(), "amr_behavior_manager_node init done");
    }

private:
    void handleTaskCommand(const TaskCommand::SharedPtr cmd)
    {
        if(!cmd)
            return;

        if(cmd->command_type == "TASK_ASSIGN")
        {
            handleTaskAssign(*cmd);
        }
        else if(cmd->command_type == "TASK_CANCEL")
        {
            handleTaskCancel(*cmd);
        }
        else if(cmd->command_type == "TASK_STATE_UPDATE")
        {
            handleTaskStateUpdate(*cmd);
        }
        else if(cmd->command_type == "GOODS_RESPONSE")
        {
            handleGoodsResponse(*cmd);
        }
    }

    void handleTaskAssign(const TaskCommand& cmd)
    {
        if(!m_map_ready)
        {
            RCLCPP_WARN(get_logger(), "ignore task because map is not ready");
            return;
        }

        if(!m_current_task_id.empty() && m_current_task_id == cmd.task_id)
        {
            RCLCPP_WARN(get_logger(), "task already assigned task=%s", cmd.task_id.c_str());
            return;
        }

        m_current_task_id = cmd.task_id;
        m_current_warehouse_id = cmd.warehouse_id;
        m_current_task_type = cmd.task_type;
        m_task_state = "ACCEPTED";
        m_goods_status.clear();
        m_current_goods.clear();
        m_waiting_goods_response = false;

        publishTaskEvent("TASK_ACCEPTED", "ACCEPTED", {}, "", "");
        RCLCPP_INFO(get_logger(), "task assigned task=%s type=%s", cmd.task_id.c_str(), cmd.task_type.c_str());
    }

    void handleTaskCancel(const TaskCommand& cmd)
    {
        if(m_current_task_id != cmd.task_id)
            return;

        if(m_nav_client)
            m_nav_client->cancelNavigation();

        m_task_state = "CANCELLED";
        publishTaskEvent("CARRY_ABORTED", "INTERRUPTED", m_current_goods, "", "TASK_CANCELLED");
        resetTask();
    }

    void handleTaskStateUpdate(const TaskCommand& cmd)
    {
        if(cmd.task_id != m_current_task_id)
            return;

        if(cmd.state == "SUCCEEDED" || cmd.state == "CANCELLED" ||
           cmd.state == "FAILED" || cmd.state == "INTERRUPTED")
        {
            m_task_state = "IDLE";
            resetTask();
            return;
        }

        m_goods_status = cmd.goods_status;
        maybeRequestNextGoods();
    }

    void handleGoodsResponse(const TaskCommand& cmd)
    {
        if(cmd.task_id != m_current_task_id || !m_waiting_goods_response)
            return;

        m_waiting_goods_response = false;
        if(!cmd.accepted)
        {
            RCLCPP_WARN(get_logger(), "goods request rejected reason=%s", cmd.reason.c_str());
            return;
        }

        m_current_goods = cmd.goods;
        beginGraspThenNavigate();
    }

    void maybeRequestNextGoods()
    {
        if(!m_edge_online)
            return;

        if(!m_map_ready)
            return;

        if(m_task_state == "CANCELLED" || m_task_state == "INTERRUPTED" || m_waiting_goods_response)
            return;

        if(m_current_task_id.empty())
            return;

        int capacity = this->get_parameter("carry_capacity").as_int();
        if(capacity <= 0)
        {
            RCLCPP_ERROR(get_logger(), "carry_capacity invalid");
            return;
        }

        for(const auto& status : m_goods_status)
        {
            if(status.pending_quantity > 0)
            {
                int requestQuantity = std::min(status.pending_quantity, capacity);
                m_request_id = makeRequestId();
                publishTaskEvent(
                    "GOODS_REQUEST",
                    "ACCEPTED",
                    {makeGoods(status.goods_id, status.goods_name, requestQuantity)},
                    m_request_id,
                    "");
                m_waiting_goods_response = true;
                RCLCPP_INFO(
                    get_logger(),
                    "request goods goods=%s quantity=%d request=%s",
                    status.goods_id.c_str(),
                    requestQuantity,
                    m_request_id.c_str());
                return;
            }
        }
    }

    void beginGraspThenNavigate()
    {
        m_task_state = "GRASPING";
        publishTaskEvent("STATUS", "GRASPING", m_current_goods, "", "");

        double graspSeconds = this->get_parameter("grasp_simulate_seconds").as_double();
        auto timer = this->create_wall_timer(
            std::chrono::duration<double>(graspSeconds),
            [this]()
            {
                m_task_state = "TRANSPORTING";
                publishTaskEvent("STATUS", "TRANSPORTING", m_current_goods, "", "");
                navigateToGoodsDestination();
            });
        m_one_shot_timers.push_back(timer);
    }

    void navigateToGoodsDestination()
    {
        if(!m_nav_client)
            return;

        geometry_msgs::msg::PoseStamped goal;
        goal.header.frame_id = "map";
        goal.header.stamp = this->now();
        goal.pose.position.x = 0.0;
        goal.pose.position.y = 0.0;
        goal.pose.orientation.w = 1.0;

        if(!m_current_goods.empty() && m_goods_point_resolver)
        {
            LocationInfo location;
            if(m_goods_point_resolver->resolve(
                m_current_goods.front().goods_id,
                m_current_task_type,
                location))
            {
                goal.pose.position.x = location.x;
                goal.pose.position.y = location.y;

                double halfYaw = location.yaw * 0.5;
                goal.pose.orientation.z = std::sin(halfYaw);
                goal.pose.orientation.w = std::cos(halfYaw);
                RCLCPP_INFO(
                    get_logger(),
                    "resolve goods point goods=%s location=%s x=%.2f y=%.2f yaw=%.2f",
                    m_current_goods.front().goods_id.c_str(),
                    location.id.c_str(),
                    location.x,
                    location.y,
                    location.yaw);
            }
            else
            {
                RCLCPP_WARN(get_logger(), "resolve goods point failed, use default point");
            }
        }

        m_nav_client->navigateToPose(goal);
    }

    void onNavigationResult(bool success)
    {
        if(m_task_state != "TRANSPORTING")
            return;

        if(!success)
        {
            m_task_state = "INTERRUPTED";
            publishTaskEvent("CARRY_ABORTED", "INTERRUPTED", m_current_goods, "", "NAVIGATION_FAILED");
            return;
        }

        m_task_state = "PLACING";
        publishTaskEvent("STATUS", "PLACING", m_current_goods, "", "");

        double placeSeconds = this->get_parameter("place_simulate_seconds").as_double();
        auto timer = this->create_wall_timer(
            std::chrono::duration<double>(placeSeconds),
            [this]()
            {
                publishTaskEvent("CARRY_COMPLETED", "TRANSPORTING", m_current_goods, "", "");
                m_current_goods.clear();
                if(m_edge_online)
                {
                    m_task_state = "ACCEPTED";
                    maybeRequestNextGoods();
                }
                else
                {
                    m_task_state = "IDLE";
                    resetTask();
                }
            });
        m_one_shot_timers.push_back(timer);
    }

    void publishPeriodicTaskStatus()
    {
        if(m_current_task_id.empty())
            return;

        publishTaskEvent("STATUS", m_task_state, m_current_goods, "", "");
    }

    void publishRobotStatus()
    {
        RobotStatus rs;
        rs.robot_id = this->get_parameter("robot_id").as_string();
        rs.stamp = this->now();

        auto pose = m_nav_client ? m_nav_client->getLatestPose() : nullptr;
        if(pose && pose->pose_valid)
        {
            rs.position_x = pose->pose.pose.position.x;
            rs.position_y = pose->pose.pose.position.y;
            rs.yaw = 0.0;
        }

        rs.state = m_task_state == "TRANSPORTING" ? RobotStatus::STATE_EXECUTING : RobotStatus::STATE_IDLE;
        rs.battery = 100.0f;
        rs.navigating = m_task_state == "TRANSPORTING";
        rs.current_task_id = m_current_task_id;
        m_robot_status_pub->publish(rs);

        publishComponentStatus();
    }

    void publishComponentStatus()
    {
        ComponentStatus msg;
        msg.stamp = this->now();

        if(!m_map_ready)
            msg.state = ComponentStatus::STATE_INITIALIZING;
        else if(m_task_state == "INTERRUPTED")
            msg.state = ComponentStatus::STATE_ERROR;
        else if(m_task_state == "ACCEPTED" ||
                m_task_state == "GRASPING" ||
                m_task_state == "TRANSPORTING" ||
                m_task_state == "PLACING")
            msg.state = ComponentStatus::STATE_WORKING;
        else
            msg.state = ComponentStatus::STATE_IDLE;

        m_component_status_pub->publish(msg);
    }

    void publishTaskEvent(
        const std::string& event_type,
        const std::string& task_state,
        const std::vector<TaskGoods>& goods,
        const std::string& request_id,
        const std::string& reason)
    {
        TaskEvent event;
        event.event_type = event_type;
        event.task_id = m_current_task_id;
        event.warehouse_id = m_current_warehouse_id;
        event.task_state = task_state;
        event.goods = goods;
        event.request_id = request_id;
        event.reason = reason;
        event.stamp = this->now();
        m_task_event_pub->publish(event);
    }

    TaskGoods makeGoods(const std::string& goods_id, const std::string& goods_name, int quantity)
    {
        TaskGoods goods;
        goods.goods_id = goods_id;
        goods.goods_name = goods_name;
        goods.total_quantity = quantity;
        return goods;
    }

    std::string makeRequestId()
    {
        return m_current_task_id + "_" + std::to_string(this->now().nanoseconds());
    }

    void resetTask()
    {
        m_current_task_id.clear();
        m_current_warehouse_id.clear();
        m_current_task_type.clear();
        m_task_state = "IDLE";
        m_goods_status.clear();
        m_current_goods.clear();
        m_request_id.clear();
        m_waiting_goods_response = false;
    }

    std::shared_ptr<NavigationClient> m_nav_client;
    std::shared_ptr<ZoneStationLoader> m_zone_station_loader;
    std::shared_ptr<GoodsPointResolver> m_goods_point_resolver;
    rclcpp::Publisher<RobotStatus>::SharedPtr m_robot_status_pub;
    rclcpp::Publisher<TaskStatus>::SharedPtr m_task_status_pub;
    rclcpp::Publisher<ComponentStatus>::SharedPtr m_component_status_pub;
    rclcpp::Subscription<TaskCommand>::SharedPtr m_task_command_sub;
    rclcpp::Publisher<TaskEvent>::SharedPtr m_task_event_pub;
    rclcpp::Subscription<amr_interfaces::msg::MapInfo>::SharedPtr m_map_info_sub;
    rclcpp::Subscription<amr_interfaces::msg::EdgeStatus>::SharedPtr m_edge_status_sub;
    rclcpp::TimerBase::SharedPtr m_robot_status_timer;
    rclcpp::TimerBase::SharedPtr m_task_status_timer;
    std::vector<rclcpp::TimerBase::SharedPtr> m_one_shot_timers;

    std::string m_current_task_id;
    std::string m_current_warehouse_id;
    std::string m_current_task_type;
    std::string m_task_state = "IDLE";
    std::string m_request_id;
    bool m_waiting_goods_response = false;
    bool m_map_ready = false;
    bool m_edge_online = true;
    std::vector<TaskGoodsStatus> m_goods_status;
    std::vector<TaskGoods> m_current_goods;
};

}

int main(int argc, char** argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<amr_behavior_manager::AmrBehaviorManagerNode>();
    node->init();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
