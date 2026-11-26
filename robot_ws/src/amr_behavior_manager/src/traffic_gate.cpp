#include "amr_behavior_manager/traffic_gate.h"

#include <cmath>
#include <limits>

#include "amr_common/ros_interface_names.h"
#include "amr_behavior_manager/traffic_gate_loader.h"
#include "amr_behavior_manager/navigation_client.h"

namespace amr_behavior_manager
{

TrafficGate::TrafficGate(
    rclcpp::Node::SharedPtr node,
    TrafficGateLoader* loader,
    NavigationClient* navigation)
    : m_node(node),
      m_loader(loader),
      m_navigation(navigation)
{
    m_request_pub = m_node->create_publisher<TrafficRequest>(
        amr_common::topic::TRAFFIC_REQUEST, 10);
    m_response_sub = m_node->create_subscription<TrafficResponse>(
        amr_common::topic::TRAFFIC_RESPONSE,
        10,
        [this](const TrafficResponse::SharedPtr response)
        {
            if(response)
                onResponse(*response);
        });

    m_retry_timer = m_node->create_wall_timer(
        std::chrono::seconds(1),
        [this]()
        {
            retryTick();
        });
}

void TrafficGate::onGoalStarted(const geometry_msgs::msg::PoseStamped& goal)
{
    m_active_goal = goal;
    m_has_active_goal = true;
}

void TrafficGate::onFeedback(const geometry_msgs::msg::PoseStamped& pose)
{
    if(!m_loader)
        return;

    for(const auto& gate : m_loader->gates())
    {
        bool inside = pointInPolygon(
            pose.pose.position.x,
            pose.pose.position.y,
            gate.polygon);

        auto& ctx = m_gates[gate.resource_id];
        ctx.resource_id = gate.resource_id;

        if(inside && !ctx.inside_prev && !ctx.already_handled)
        {
            auto axis = longAxis(gate.polygon);
            auto center = centroid(gate.polygon);
            double s = (pose.pose.position.x - center.first) * axis.first +
                       (pose.pose.position.y - center.second) * axis.second;
            ctx.entry_side = s < 0.0 ? "l1" : "l2";
            chooseWaitPose(gate, ctx.entry_side, pose, ctx.wait_pose);
            ctx.request_id = std::string(m_node->get_name()) + "_" + gate.resource_id + "_" +
                             std::to_string(static_cast<long long>(m_node->now().nanoseconds()));
            ctx.epoch++;
            ctx.state = "REQUESTING";
            ctx.last_request_at = m_node->now();
            sendRequest(gate.resource_id, "REQUEST");
        }
        else if(!inside && ctx.inside_prev)
        {
            if(ctx.state == "HELD" || ctx.state == "RESUMING" || ctx.state == "REQUESTING")
            {
                sendRequest(gate.resource_id, "RELEASE");
                ctx.state = "RELEASED";
                ctx.already_handled = true;
            }
        }

        ctx.inside_prev = inside;
    }
}

void TrafficGate::onResponse(const TrafficResponse& response)
{
    auto it = m_gates.find(response.resource_id);
    if(it == m_gates.end())
        return;

    auto& ctx = it->second;
    if(response.epoch != ctx.epoch)
    {
        RCLCPP_WARN(m_node->get_logger(), "drop stale traffic response resource=%s", response.resource_id.c_str());
        return;
    }

    if(response.result == "GRANTED")
    {
        if(ctx.state == "REQUESTING")
        {
            ctx.state = "HELD";
            RCLCPP_INFO(m_node->get_logger(), "traffic early granted resource=%s", response.resource_id.c_str());
        }
        else if(ctx.state == "WAITING")
        {
            if(m_has_active_goal)
            {
                m_navigation->navigateToPose(m_active_goal);
            }
            ctx.state = "RESUMING";
            RCLCPP_INFO(m_node->get_logger(), "traffic granted while waiting, resume D resource=%s", response.resource_id.c_str());
        }
    }
    else if(response.result == "PENDING" || response.result == "REJECTED")
    {
        if(ctx.state == "REQUESTING")
        {
            m_navigation->cancelNavigation();
            ctx.state = "CANCELING";
            RCLCPP_WARN(m_node->get_logger(), "traffic rejected, cancel navigation resource=%s", response.resource_id.c_str());
        }
    }
    else if(response.result == "RELEASED" || response.result == "CANCELLED" ||
            response.result == "EXPIRED")
    {
        ctx.state = "RELEASED";
    }
}

void TrafficGate::onNavigationResult(bool success)
{
    (void)success;

    for(auto& [resource_id, ctx] : m_gates)
    {
        if(ctx.state == "CANCELING")
        {
            if(!ctx.wait_pose.header.frame_id.empty())
            {
                m_navigation->navigateToPose(ctx.wait_pose);
                ctx.state = "WAITING";
                ctx.last_request_at = m_node->now();
                RCLCPP_INFO(m_node->get_logger(), "traffic wait pose sent resource=%s", resource_id.c_str());
            }
        }
        else if(ctx.state == "RESUMING")
        {
            ctx.state = "HELD";
        }
    }
}

void TrafficGate::onTaskFinished(bool success)
{
    (void)success;
    releaseAll();
}

void TrafficGate::onTaskCancelled()
{
    releaseAll();
}

void TrafficGate::retryTick()
{
    auto now = m_node->now();
    for(auto& [resource_id, ctx] : m_gates)
    {
        if(ctx.state == "WAITING" &&
           (now - ctx.last_request_at).seconds() >= 10.0)
        {
            ctx.last_request_at = now;
            sendRequest(resource_id, "REQUEST");
            RCLCPP_INFO(m_node->get_logger(), "traffic retry request resource=%s", resource_id.c_str());
        }
    }
}

void TrafficGate::chooseWaitPose(
    const TrafficGateData& gate,
    const std::string& entry_side,
    const geometry_msgs::msg::PoseStamped& current,
    geometry_msgs::msg::PoseStamped& wait_pose) const
{
    auto it = gate.wait_poses.find(entry_side);
    if(it == gate.wait_poses.end() || it->second.empty())
        return;

    const auto& poses = it->second;
    double best = std::numeric_limits<double>::max();
    for(const auto& pose : poses)
    {
        double dx = pose.x - current.pose.position.x;
        double dy = pose.y - current.pose.position.y;
        double dist = dx * dx + dy * dy;
        if(dist < best)
        {
            best = dist;
            wait_pose.header.frame_id = "map";
            wait_pose.header.stamp = m_node->now();
            wait_pose.pose.position.x = pose.x;
            wait_pose.pose.position.y = pose.y;
            double half = pose.yaw * 0.5;
            wait_pose.pose.orientation.z = std::sin(half);
            wait_pose.pose.orientation.w = std::cos(half);
        }
    }
}

bool TrafficGate::pointInPolygon(
    double x,
    double y,
    const std::vector<std::pair<double,double>>& polygon)
{
    bool inside = false;
    std::size_t n = polygon.size();
    for(std::size_t i = 0, j = n - 1; i < n; j = i++)
    {
        const auto& pi = polygon[i];
        const auto& pj = polygon[j];
        if(((pi.second > y) != (pj.second > y)) &&
           (x < (pj.first - pi.first) * (y - pi.second) / (pj.second - pi.second) + pi.first))
            inside = !inside;
    }
    return inside;
}

std::pair<double,double> TrafficGate::centroid(
    const std::vector<std::pair<double,double>>& polygon)
{
    double x = 0.0;
    double y = 0.0;
    for(const auto& p : polygon)
    {
        x += p.first;
        y += p.second;
    }
    double n = polygon.empty() ? 1.0 : static_cast<double>(polygon.size());
    return {x / n, y / n};
}

std::pair<double,double> TrafficGate::longAxis(
    const std::vector<std::pair<double,double>>& polygon)
{
    if(polygon.size() < 4)
        return {0.0, 0.0};
    double dx = polygon[2].first - polygon[1].first;
    double dy = polygon[2].second - polygon[1].second;
    double len = std::sqrt(dx * dx + dy * dy);
    if(len < 1e-6)
        return {0.0, 0.0};
    return {dx / len, dy / len};
}

double TrafficGate::distance(
    const std::pair<double,double>& a,
    const std::pair<double,double>& b)
{
    double dx = a.first - b.first;
    double dy = a.second - b.second;
    return std::sqrt(dx * dx + dy * dy);
}

void TrafficGate::sendRequest(const std::string& resource_id, const std::string& op)
{
    auto it = m_gates.find(resource_id);
    if(it == m_gates.end())
        return;

    TrafficRequest request;
    request.op = op;
    request.request_id = it->second.request_id;
    request.robot_id = m_node->get_namespace();
    if(request.robot_id.size() > 1 && request.robot_id.front() == '/')
        request.robot_id.erase(request.robot_id.begin());
    request.resource_id = resource_id;
    request.epoch = it->second.epoch;
    request.eta_sec = 0.0;
    request.lease_ttl_sec = 60.0;
    m_request_pub->publish(request);
}

void TrafficGate::releaseAll()
{
    for(auto& [resource_id, ctx] : m_gates)
    {
        if(ctx.state == "HELD" || ctx.state == "RESUMING" || ctx.state == "REQUESTING" ||
           ctx.state == "WAITING")
        {
            sendRequest(resource_id, "RELEASE");
            ctx.state = "RELEASED";
        }
    }
}

}
