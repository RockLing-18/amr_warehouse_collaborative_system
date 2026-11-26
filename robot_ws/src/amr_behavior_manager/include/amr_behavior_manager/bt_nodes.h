#pragma once

#include <functional>
#include <memory>

#include "geometry_msgs/msg/pose_stamped.hpp"

namespace BT
{
class BehaviorTreeFactory;
}

namespace amr_behavior_manager
{

struct BtContext
{
    std::function<bool()> goods_approved;
    std::function<bool()> traffic_granted;
    std::function<bool()> battery_low;
    std::function<bool(const geometry_msgs::msg::PoseStamped&)> navigate;
    std::function<void()> grasp;
    std::function<void()> place;
    std::function<void(bool)> report;
    std::function<void()> release_traffic_right;
    std::function<int()> navigation_status;   // 0=RUNNING 1=SUCCESS 2=FAILED/CANCELED
    std::function<void()> cancel_navigation;
};

void registerAmrBtNodes(
    BT::BehaviorTreeFactory& factory,
    std::shared_ptr<BtContext> context);

}
