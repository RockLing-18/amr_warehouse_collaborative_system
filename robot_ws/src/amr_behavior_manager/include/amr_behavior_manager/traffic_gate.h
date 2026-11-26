#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <chrono>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "amr_interfaces/msg/traffic_request.hpp"
#include "amr_interfaces/msg/traffic_response.hpp"

namespace amr_behavior_manager
{

class NavigationClient;
class TrafficGateLoader;
struct TrafficGateData;

class TrafficGate
{
public:
    using TrafficRequest = amr_interfaces::msg::TrafficRequest;
    using TrafficResponse = amr_interfaces::msg::TrafficResponse;

    TrafficGate(
        rclcpp::Node::SharedPtr node,
        TrafficGateLoader* loader,
        NavigationClient* navigation);

    void onFeedback(const geometry_msgs::msg::PoseStamped& pose);
    void onResponse(const TrafficResponse& response);
    void onGoalStarted(const geometry_msgs::msg::PoseStamped& goal);
    void onNavigationResult(bool success);
    void onTaskFinished(bool success);
    void onTaskCancelled();

private:
    struct GateContext
    {
        std::string resource_id;
        std::string request_id;
        int64_t epoch = 0;
        std::string state = "IDLE";
        std::string entry_side;
        bool inside_prev = false;
        bool already_handled = false;
        geometry_msgs::msg::PoseStamped wait_pose;
        rclcpp::Time last_request_at;
    };

    static bool pointInPolygon(
        double x,
        double y,
        const std::vector<std::pair<double,double>>& polygon);
    static std::pair<double,double> centroid(
        const std::vector<std::pair<double,double>>& polygon);
    static std::pair<double,double> longAxis(
        const std::vector<std::pair<double,double>>& polygon);
    static double distance(
        const std::pair<double,double>& a,
        const std::pair<double,double>& b);

    void sendRequest(const std::string& resource_id, const std::string& op);
    void chooseWaitPose(
        const TrafficGateData& gate,
        const std::string& entry_side,
        const geometry_msgs::msg::PoseStamped& current,
        geometry_msgs::msg::PoseStamped& wait_pose) const;
    void retryTick();
    void releaseAll();

private:
    rclcpp::Node::SharedPtr m_node;
    TrafficGateLoader* m_loader;
    NavigationClient* m_navigation;

    rclcpp::Publisher<TrafficRequest>::SharedPtr m_request_pub;
    rclcpp::Subscription<TrafficResponse>::SharedPtr m_response_sub;
    rclcpp::TimerBase::SharedPtr m_retry_timer;
    std::unordered_map<std::string, GateContext> m_gates;
    geometry_msgs::msg::PoseStamped m_active_goal;
    bool m_has_active_goal{false};
};

}
