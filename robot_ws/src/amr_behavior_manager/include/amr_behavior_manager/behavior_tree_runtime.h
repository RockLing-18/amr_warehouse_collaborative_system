#pragma once

#include <memory>
#include <string>

#include "behaviortree_cpp/bt_factory.h"
#include "amr_behavior_manager/bt_nodes.h"
#include "geometry_msgs/msg/pose_stamped.hpp"

namespace amr_behavior_manager
{

class BehaviorTreeRuntime
{
public:
    bool init(const std::string& xml_path);

    void setGoodsApproved(bool value);
    void setTrafficGranted(bool value);
    void setBatteryLow(bool value);
    void setGoal(const geometry_msgs::msg::PoseStamped& goal);
    std::shared_ptr<BtContext> context();

    BT::NodeStatus tickOnce();

private:
    BT::BehaviorTreeFactory m_factory;
    std::unique_ptr<BT::Tree> m_tree;
    BT::Blackboard::Ptr m_blackboard;
    std::shared_ptr<BtContext> m_context;
};

}
