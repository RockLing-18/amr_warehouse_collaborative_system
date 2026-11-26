#include <chrono>
#include <atomic>
#include <cmath>
#include <memory>
#include <string>
#include <algorithm>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"

#include "amr_behavior_manager/navigation_client.h"
#include "amr_behavior_manager/zone_station_loader.h"
#include "amr_behavior_manager/goods_point_resolver.h"
#include "amr_behavior_manager/traffic_gate_loader.h"
#include "amr_behavior_manager/traffic_gate.h"
#include "amr_behavior_manager/behavior_tree_runtime.h"
#include "amr_common/ros_interface_names.h"
#include "amr_interfaces/msg/robot_status.hpp"
#include "amr_interfaces/msg/task_status.hpp"
#include "amr_interfaces/msg/task_command.hpp"
#include "amr_interfaces/msg/task_event.hpp"
#include "amr_interfaces/msg/map_info.hpp"
#include "amr_interfaces/msg/edge_status.hpp"
#include "amr_interfaces/msg/component_status.hpp"
#include "amr_interfaces/msg/initial_pose.hpp"
#include "amr_interfaces/msg/power_mode.hpp"
#include "amr_interfaces/msg/battery_state.hpp"
#include "ament_index_cpp/get_package_share_directory.hpp"

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
    using InitialPose = amr_interfaces::msg::InitialPose;
    using PowerMode = amr_interfaces::msg::PowerMode;
    using BatteryState = amr_interfaces::msg::BatteryState;

    AmrBehaviorManagerNode()
        : Node("amr_behavior_manager")
    {
        this->declare_parameter<std::string>("robot_id", "robot_01");
        this->declare_parameter<double>("status_publish_rate_hz", 1.0);
        this->declare_parameter<double>("task_status_period_s", 5.0);
        this->declare_parameter<double>("grasp_simulate_seconds", 1.0);
        this->declare_parameter<double>("place_simulate_seconds", 1.0);
        this->declare_parameter<int>("carry_capacity", 10);
        this->declare_parameter<bool>("use_bt_execution", false);

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
        m_power_mode_pub = this->create_publisher<PowerMode>(
            amr_common::topic::POWER_MODE, 10);
        m_battery_state_sub = this->create_subscription<BatteryState>(
            amr_common::topic::POWER_LEVEL,
            10,
            [this](const BatteryState::SharedPtr msg)
            {
                if(msg)
                    m_battery = msg->battery;
            });

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
                    if(!m_traffic_gate_loader)
                        m_traffic_gate_loader = std::make_shared<TrafficGateLoader>();
                    m_traffic_gate_loader->load(msg->traffic_gates_yaml);
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

        auto initial_pose_qos = rclcpp::QoS(rclcpp::KeepLast(1));
        initial_pose_qos.reliable();
        initial_pose_qos.transient_local();
        m_initial_pose_sub = this->create_subscription<InitialPose>(
            amr_common::topic::INITIAL_POSE,
            initial_pose_qos,
            [this](const InitialPose::SharedPtr pose)
            {
                if(!pose)
                    return;
                m_home_pose = pose;
                RCLCPP_INFO(
                    get_logger(),
                    "receive home pose x=%.2f y=%.2f yaw=%.2f",
                    pose->x, pose->y, pose->yaw);
            });
    }

    void init()
    {
        bool useBtExecution = this->get_parameter("use_bt_execution").as_bool();
        RCLCPP_INFO(get_logger(), "use_bt_execution=%s",
            useBtExecution ? "true" : "false");

        try
        {
            std::string xmlPath =
                ament_index_cpp::get_package_share_directory("amr_behavior_manager") +
                "/config/behavior_trees/nav_leg_tree.xml";
            m_bt_runtime = std::make_shared<BehaviorTreeRuntime>();
            if(m_bt_runtime->init(xmlPath))
            {
                auto ctx = m_bt_runtime->context();
                ctx->goods_approved = [this]()
                {
                    return !m_current_goods.empty();
                };
                ctx->traffic_granted = [this]()
                {
                    return true;
                };
                ctx->battery_low = [this]()
                {
                    return m_battery <= 20.0f;
                };
                ctx->navigate = [this](const geometry_msgs::msg::PoseStamped& goal)
                {
                    if(!m_nav_client)
                        return false;
                    m_bt_nav_status = 0;
                    return m_nav_client->navigateToPose(goal);
                };
                ctx->navigation_status = [this]()
                {
                    return m_bt_nav_status.load();
                };
                ctx->cancel_navigation = [this]()
                {
                    if(m_nav_client)
                        m_nav_client->cancelNavigation();
                };
                ctx->grasp = [this]()
                {
                    publishTaskEvent("STATUS", "GRASPING", m_current_goods, "", "");
                };
                ctx->place = [this]()
                {
                    publishTaskEvent("STATUS", "PLACING", m_current_goods, "", "");
                };
                ctx->report = [this](bool success)
                {
                    if(success)
                        publishTaskEvent("CARRY_COMPLETED", "TRANSPORTING", m_current_goods, m_request_id, "");
                    else
                        publishTaskEvent("CARRY_ABORTED", "INTERRUPTED", m_current_goods, m_request_id, "NAVIGATION_FAILED");
                };
                ctx->release_traffic_right = [this]()
                {
                    if(m_traffic_gate)
                        m_traffic_gate->onTaskFinished(true);
                };

                if(this->get_parameter("use_bt_execution").as_bool())
                {
                    m_bt_timer = this->create_wall_timer(
                        std::chrono::milliseconds(100),
                        [this]()
                        {
                            if(!m_bt_runtime)
                                return;
                            if(!m_bt_nav_active)
                                return;
                            m_bt_runtime->setGoodsApproved(!m_current_goods.empty());
                            m_bt_runtime->setTrafficGranted(true);
                            m_bt_runtime->setBatteryLow(m_battery <= 20.0f);
                            m_bt_runtime->tickOnce();
                        });
                }
            }
        }
        catch(const std::exception& e)
        {
            RCLCPP_WARN(get_logger(), "init behavior tree failed: %s", e.what());
        }

        m_traffic_gate_loader = std::make_shared<TrafficGateLoader>();
        m_nav_client = std::make_shared<NavigationClient>(shared_from_this());
        m_traffic_gate = std::make_shared<TrafficGate>(
            shared_from_this(), m_traffic_gate_loader.get(), m_nav_client.get());
        m_nav_client->setFeedbackCallback(
            [this](const std::shared_ptr<const NavigationClient::AmrNavigateToPose::Feedback> feedback)
            {
                if(feedback && m_traffic_gate)
                    m_traffic_gate->onFeedback(feedback->current_pose);
            });

        m_nav_client->setResultCallback(
            [this](const NavigationClient::GoalHandle::WrappedResult& result)
            {
                bool success = result.code == rclcpp_action::ResultCode::SUCCEEDED;
                m_bt_nav_status = success ? 1 : 2;
                m_bt_nav_active = false;
                if(m_traffic_gate)
                    m_traffic_gate->onNavigationResult(success);
                if(m_returning_home)
                    onReturnHomeResult(success);
                else
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

        if(m_returning_home)
        {
            m_returning_home = false;
            if(m_nav_client)
                m_nav_client->cancelNavigation();
        }
        if(m_home_retry_timer)
        {
            m_home_retry_timer->cancel();
            m_home_retry_timer.reset();
        }
        m_home_retry_count = 0;

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
        if(m_traffic_gate)
            m_traffic_gate->onTaskCancelled();

        m_task_state = "CANCELLED";
        if(!m_request_id.empty())
            publishTaskEvent("CARRY_ABORTED", "CANCELLED", m_current_goods, m_request_id, "TASK_CANCELLED");
        else
            publishTaskEvent("STATUS", "CANCELLED", {}, "", "TASK_CANCELLED");
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
            returnToHome();
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

        // 只在空闲等待下一趟(ACCEPTED)时申请货物，避免 GRASPING/TRANSPORTING/PLACING 期间重复申请
        if(m_task_state != "ACCEPTED" || m_waiting_goods_response)
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
        m_task_state = "NAVIGATING_TO_PICKUP";
        publishTaskEvent("STATUS", "NAVIGATING_TO_PICKUP", m_current_goods, "", "");
        navigateToGoodsPoint(false);
    }

    void navigateToGoodsPoint(bool toDropoff)
    {
        if(!m_nav_client)
        {
            RCLCPP_ERROR(get_logger(), "navigation client not available, abort carry");
            onNavigationResult(false);
            return;
        }

        geometry_msgs::msg::PoseStamped goal;
        goal.header.frame_id = "map";
        goal.header.stamp = this->now();
        goal.pose.position.x = 0.0;
        goal.pose.position.y = 0.0;
        goal.pose.orientation.w = 1.0;

        if(!m_current_goods.empty() && m_goods_point_resolver)
        {
            std::string zone_type;
            if(m_current_task_type == "LOAD")
                zone_type = toDropoff ? "LOAD" : "STORAGE";
            else
                zone_type = toDropoff ? "STORAGE" : "UNLOAD";

            LocationInfo location;
            if(m_goods_point_resolver->resolveForZoneType(
                m_current_goods.front().goods_id,
                zone_type,
                location))
            {
                goal.pose.position.x = location.x;
                goal.pose.position.y = location.y;

                double halfYaw = location.yaw * 0.5;
                goal.pose.orientation.z = std::sin(halfYaw);
                goal.pose.orientation.w = std::cos(halfYaw);
                RCLCPP_INFO(
                    get_logger(),
                    "resolve goods point phase=%s zone=%s goods=%s location=%s x=%.2f y=%.2f yaw=%.2f",
                    toDropoff ? "dropoff" : "pickup",
                    zone_type.c_str(),
                    m_current_goods.front().goods_id.c_str(),
                    location.id.c_str(),
                    location.x,
                    location.y,
                    location.yaw);
            }
            else
            {
                RCLCPP_ERROR(get_logger(), "resolve goods point failed phase=%s, abort carry",
                    toDropoff ? "dropoff" : "pickup");
                onNavigationResult(false);
                return;
            }
        }
        else
        {
            RCLCPP_ERROR(get_logger(), "no goods or point resolver, abort carry");
            onNavigationResult(false);
            return;
        }

        if(m_traffic_gate)
            m_traffic_gate->onGoalStarted(goal);

        if(this->get_parameter("use_bt_execution").as_bool())
        {
            if(!m_bt_runtime)
            {
                RCLCPP_ERROR(get_logger(), "behavior tree runtime unavailable, abort carry");
                onNavigationResult(false);
                return;
            }
            m_bt_runtime->setGoal(goal);
            m_bt_nav_active = true;
            return;
        }

        if(!m_nav_client->navigateToPose(goal))
        {
            RCLCPP_ERROR(get_logger(), "navigation action server unavailable, abort carry");
            onNavigationResult(false);
        }
    }

    void onNavigationResult(bool success)
    {
        if(m_traffic_gate)
            m_traffic_gate->onTaskFinished(success);

        if(m_task_state != "NAVIGATING_TO_PICKUP" && m_task_state != "TRANSPORTING")
            return;

        if(!success)
        {
            m_task_state = "INTERRUPTED";
            publishTaskEvent("CARRY_ABORTED", "INTERRUPTED", m_current_goods, m_request_id, "NAVIGATION_FAILED");
            return;
        }

        if(m_task_state == "NAVIGATING_TO_PICKUP")
        {
            m_task_state = "GRASPING";
            publishTaskEvent("STATUS", "GRASPING", m_current_goods, "", "");

            double graspSeconds = this->get_parameter("grasp_simulate_seconds").as_double();
            auto timer = std::make_shared<rclcpp::TimerBase::SharedPtr>();
            *timer = this->create_wall_timer(
                std::chrono::duration<double>(graspSeconds),
                [this, timer]()
                {
                    if(*timer)
                        (*timer)->cancel();

                    if(m_task_state != "GRASPING")
                        return;

                    m_task_state = "TRANSPORTING";
                    publishTaskEvent("STATUS", "TRANSPORTING", m_current_goods, "", "");
                    navigateToGoodsPoint(true);
                });
            m_one_shot_timers.push_back(*timer);
            return;
        }

        m_task_state = "PLACING";
        publishTaskEvent("STATUS", "PLACING", m_current_goods, "", "");

        double placeSeconds = this->get_parameter("place_simulate_seconds").as_double();
        auto timer = std::make_shared<rclcpp::TimerBase::SharedPtr>();
        *timer = this->create_wall_timer(
            std::chrono::duration<double>(placeSeconds),
            [this, timer]()
            {
                if(*timer)
                    (*timer)->cancel();

                if(m_task_state != "PLACING")
                    return;

                publishTaskEvent("CARRY_COMPLETED", "TRANSPORTING", m_current_goods, m_request_id, "");
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
        m_one_shot_timers.push_back(*timer);
    }

    void returnToHome()
    {
        if(m_returning_home)
            return;

        if(!m_nav_client)
        {
            RCLCPP_WARN(get_logger(), "navigation client not available, skip return home");
            return;
        }

        if(!m_home_pose)
        {
            RCLCPP_WARN(get_logger(), "home pose not received, skip return home");
            return;
        }

        geometry_msgs::msg::PoseStamped goal;
        goal.header.frame_id = "map";
        goal.header.stamp = this->now();
        goal.pose.position.x = m_home_pose->x;
        goal.pose.position.y = m_home_pose->y;
        double halfYaw = m_home_pose->yaw * 0.5;
        goal.pose.orientation.z = std::sin(halfYaw);
        goal.pose.orientation.w = std::cos(halfYaw);

        m_returning_home = true;
        m_task_state = "NAVIGATING_HOME";
        RCLCPP_INFO(
            get_logger(),
            "return home x=%.2f y=%.2f yaw=%.2f",
            m_home_pose->x, m_home_pose->y, m_home_pose->yaw);

        if(!m_nav_client->navigateToPose(goal))
        {
            RCLCPP_ERROR(get_logger(), "navigation action server unavailable, skip return home");
            m_returning_home = false;
            m_task_state = "IDLE";
        }
    }

    void onReturnHomeResult(bool success)
    {
        m_returning_home = false;
        m_task_state = "IDLE";
        if(success)
        {
            m_home_retry_count = 0;
            RCLCPP_INFO(get_logger(), "returned to home");
        }
        else
        {
            if(m_home_retry_count < 3)
            {
                ++m_home_retry_count;
                RCLCPP_WARN(
                    get_logger(),
                    "return home failed, retry %d/3 in 3s",
                    m_home_retry_count);
                m_home_retry_timer = this->create_wall_timer(
                    std::chrono::seconds(3),
                    [this]()
                    {
                        if(m_home_retry_timer)
                            m_home_retry_timer->cancel();
                        m_home_retry_timer.reset();
                        returnToHome();
                    });
            }
            else
            {
                m_home_retry_count = 0;
                RCLCPP_ERROR(get_logger(), "return home failed after retries");
            }
        }
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

        bool actionActive =
            m_task_state == "NAVIGATING_TO_PICKUP" ||
            m_task_state == "GRASPING" ||
            m_task_state == "TRANSPORTING" ||
            m_task_state == "PLACING";
        rs.state = actionActive ? RobotStatus::STATE_EXECUTING : RobotStatus::STATE_IDLE;
        rs.battery = m_battery;
        rs.navigating =
            m_task_state == "NAVIGATING_TO_PICKUP" ||
            m_task_state == "TRANSPORTING" ||
            m_returning_home;
        rs.current_task_id = m_current_task_id;
        m_robot_status_pub->publish(rs);

        PowerMode mode;
        mode.stamp = this->now();
        if(m_charging)
            mode.mode = PowerMode::MODE_CHARGING;
        else if(actionActive || m_returning_home)
            mode.mode = PowerMode::MODE_WORK;
        else
            mode.mode = PowerMode::MODE_IDLE;
        m_power_mode_pub->publish(mode);

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
                m_task_state == "NAVIGATING_TO_PICKUP" ||
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
    std::shared_ptr<TrafficGateLoader> m_traffic_gate_loader;
    std::shared_ptr<TrafficGate> m_traffic_gate;
    std::shared_ptr<BehaviorTreeRuntime> m_bt_runtime;
    std::shared_ptr<ZoneStationLoader> m_zone_station_loader;
    std::shared_ptr<GoodsPointResolver> m_goods_point_resolver;
    rclcpp::Publisher<RobotStatus>::SharedPtr m_robot_status_pub;
    rclcpp::Publisher<TaskStatus>::SharedPtr m_task_status_pub;
    rclcpp::Publisher<ComponentStatus>::SharedPtr m_component_status_pub;
    rclcpp::Subscription<TaskCommand>::SharedPtr m_task_command_sub;
    rclcpp::Publisher<TaskEvent>::SharedPtr m_task_event_pub;
    rclcpp::Publisher<PowerMode>::SharedPtr m_power_mode_pub;
    rclcpp::Subscription<BatteryState>::SharedPtr m_battery_state_sub;
    rclcpp::Subscription<amr_interfaces::msg::MapInfo>::SharedPtr m_map_info_sub;
    rclcpp::Subscription<amr_interfaces::msg::EdgeStatus>::SharedPtr m_edge_status_sub;
    rclcpp::Subscription<InitialPose>::SharedPtr m_initial_pose_sub;
    rclcpp::TimerBase::SharedPtr m_robot_status_timer;
    rclcpp::TimerBase::SharedPtr m_task_status_timer;
    rclcpp::TimerBase::SharedPtr m_bt_timer;
    rclcpp::TimerBase::SharedPtr m_home_retry_timer;
    std::vector<rclcpp::TimerBase::SharedPtr> m_one_shot_timers;

    std::string m_current_task_id;
    std::string m_current_warehouse_id;
    std::string m_current_task_type;
    std::string m_task_state = "IDLE";
    std::string m_request_id;
    bool m_waiting_goods_response = false;
    bool m_map_ready = false;
    bool m_edge_online = true;
    bool m_charging = false;
    bool m_returning_home = false;
    int m_home_retry_count = 0;
    InitialPose::SharedPtr m_home_pose;
    std::vector<TaskGoodsStatus> m_goods_status;
    std::vector<TaskGoods> m_current_goods;
    float m_battery = 100.0f;
    std::atomic<int> m_bt_nav_status{2};
    std::atomic<bool> m_bt_nav_active{false};
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
