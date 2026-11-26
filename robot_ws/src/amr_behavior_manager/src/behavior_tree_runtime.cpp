#include "amr_behavior_manager/behavior_tree_runtime.h"
#include "amr_behavior_manager/bt_nodes.h"

#include <iostream>

namespace amr_behavior_manager
{

bool BehaviorTreeRuntime::init(const std::string& xml_path)
{
    m_context = std::make_shared<BtContext>();
    registerAmrBtNodes(m_factory, m_context);
    m_factory.registerBehaviorTreeFromFile(xml_path);
    m_tree = std::make_unique<BT::Tree>(m_factory.createTree("NavLegTree"));
    m_blackboard = m_tree->rootBlackboard();
    m_blackboard->set("bt_context", m_context);

    setGoodsApproved(false);
    setTrafficGranted(false);
    setBatteryLow(false);
    m_blackboard->set("goal", geometry_msgs::msg::PoseStamped{});
    return true;
}

void BehaviorTreeRuntime::setGoodsApproved(bool value)
{
    if(m_blackboard)
        m_blackboard->set("goods_approved", value);
}

void BehaviorTreeRuntime::setTrafficGranted(bool value)
{
    if(m_blackboard)
        m_blackboard->set("traffic_granted", value);
}

void BehaviorTreeRuntime::setBatteryLow(bool value)
{
    if(m_blackboard)
        m_blackboard->set("battery_low", value);
}

void BehaviorTreeRuntime::setGoal(const geometry_msgs::msg::PoseStamped& goal)
{
    if(m_blackboard)
        m_blackboard->set("goal", goal);
}

std::shared_ptr<BtContext> BehaviorTreeRuntime::context()
{
    return m_context;
}

BT::NodeStatus BehaviorTreeRuntime::tickOnce()
{
    if(!m_tree)
        return BT::NodeStatus::IDLE;
    return m_tree->tickOnce();
}

}
