// amr_manager_node.cpp
// amr_manager 入口节点
//
// 发布（相对名）: robot_status / task_status
// 对接 amr_navigation 服务（接口类型来自 amr_interfaces，相对名）:
//   action : amr_navigate_to_pose
//   service: get_robot_pose
//   topic  : robot_pose
//


#include <chrono>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "amr_manager/navigation_client.h"
#include "amr_interfaces/msg/robot_status.hpp"
#include "amr_interfaces/msg/task_status.hpp"

namespace amr_manager
{

class AmrManagerNode : public rclcpp::Node
{
public:
    using RobotStatus = amr_interfaces::msg::RobotStatus;
    using TaskStatus = amr_interfaces::msg::TaskStatus;

    AmrManagerNode() : Node("amr_manager")
    {
        this->declare_parameter<std::string>("robot_id", "robot_01");
        this->declare_parameter<double>("status_publish_rate_hz", 1.0);

        // 相对名，robot namespace（如 /robot_01）
        m_robot_status_pub = this->create_publisher<RobotStatus>("robot_status", 10);
        m_task_status_pub = this->create_publisher<TaskStatus>("task_status", 10);
    }

    void init()
    {
        m_nav_client = std::make_shared<NavigationClient>(shared_from_this());
        m_nav_client->setFeedbackCallback(
            [this](const std::shared_ptr<const NavigationClient::AmrNavigateToPose::Feedback> feedback)
            {
                // 更新任务执行状态（state / distance_remaining）
                (void)feedback;
            });

        m_nav_client->setResultCallback(
            [this](const NavigationClient::GoalHandle::WrappedResult &result)
            {
               // 任务状态推进（SUCCEEDED / FAILED / CANCELED）
                (void)result;
            });

        double rate = this->get_parameter("status_publish_rate_hz").as_double();
        m_status_timer = this->create_wall_timer(
            std::chrono::duration<double>(1.0 / rate),
            std::bind(&AmrManagerNode::publishStatus, this));

        RCLCPP_INFO(this->get_logger(), "amr_manager_node init done");
    }

private:
    void publishStatus()
    {
        RobotStatus rs;
        rs.robot_id = this->get_parameter("robot_id").as_string();
        rs.stamp = this->now();

        // 位姿来自 robot_pose 订阅（无导航任务时也可用）
        auto pose = m_nav_client->getLatestPose();
        if (pose && pose->pose_valid)
        {
            rs.position_x = pose->pose.pose.position.x;
            rs.position_y = pose->pose.pose.position.y;
            // yaw 从四元数换算
            rs.yaw = 0.0;
        }

        // 1. 填充机器人运行状态（IDLE / EXECUTING / CHARGING / ERROR / SAFE_MODE）
        // 2. 填充电量、是否导航中、当前任务 id
        rs.state = RobotStatus::STATE_IDLE;
        rs.battery = 100.0f;
        rs.navigating = false;
        rs.current_task_id = "";

        m_robot_status_pub->publish(rs);

        // 发布 task_status（当前任务快照，无任务时跳过）
        // TaskStatus ts;
        // ts.task_id = ...;
        // ...
        // m_task_status_pub->publish(ts);
    }

    std::shared_ptr<NavigationClient> m_nav_client;
    rclcpp::Publisher<RobotStatus>::SharedPtr m_robot_status_pub;
    rclcpp::Publisher<TaskStatus>::SharedPtr m_task_status_pub;
    rclcpp::TimerBase::SharedPtr m_status_timer;
};

} 

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<amr_manager::AmrManagerNode>();
    node->init();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
