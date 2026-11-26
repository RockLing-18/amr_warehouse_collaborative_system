#include "amr_behavior_manager/bt_nodes.h"

#include "behaviortree_cpp/bt_factory.h"
#include "behaviortree_cpp/condition_node.h"
#include "behaviortree_cpp/action_node.h"


namespace amr_behavior_manager
{

class NavigateToPoseStatefulNode : public BT::StatefulActionNode
{
public:
    NavigateToPoseStatefulNode(const std::string& name, const BT::NodeConfig& config)
        : BT::StatefulActionNode(name, config)
    {
    }

    static BT::PortsList providedPorts()
    {
        return {BT::InputPort<geometry_msgs::msg::PoseStamped>("goal")};
    }

    BT::NodeStatus onStart() override
    {
        auto context = config().blackboard->get<std::shared_ptr<BtContext>>("bt_context");
        auto goal = getInput<geometry_msgs::msg::PoseStamped>("goal");
        if(!context || !goal || !context->navigate)
            return BT::NodeStatus::FAILURE;
        if(goal.value().header.frame_id.empty())
            return BT::NodeStatus::FAILURE;
        return context->navigate(goal.value()) ? BT::NodeStatus::RUNNING : BT::NodeStatus::FAILURE;
    }

    BT::NodeStatus onRunning() override
    {
        auto context = config().blackboard->get<std::shared_ptr<BtContext>>("bt_context");
        if(!context || !context->navigation_status)
            return BT::NodeStatus::RUNNING;
        int status = context->navigation_status();
        if(status == 0)
            return BT::NodeStatus::RUNNING;
        return status == 1 ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
    }

    void onHalted() override
    {
        auto context = config().blackboard->get<std::shared_ptr<BtContext>>("bt_context");
        if(context && context->cancel_navigation)
            context->cancel_navigation();
    }
};

void registerAmrBtNodes(
    BT::BehaviorTreeFactory& factory,
    std::shared_ptr<BtContext> context)
{
    factory.registerSimpleCondition(
        "GoodsApproved",
        [context](BT::TreeNode& node)
        {
            (void)node;
            return context && context->goods_approved && context->goods_approved()
                ? BT::NodeStatus::SUCCESS
                : BT::NodeStatus::FAILURE;
        });

    factory.registerSimpleCondition(
        "TrafficGranted",
        [context](BT::TreeNode& node)
        {
            (void)node;
            return context && context->traffic_granted && context->traffic_granted()
                ? BT::NodeStatus::SUCCESS
                : BT::NodeStatus::FAILURE;
        });

    factory.registerSimpleCondition(
        "BatteryOK",
        [context](BT::TreeNode& node)
        {
            (void)node;
            return context && context->battery_low && context->battery_low()
                ? BT::NodeStatus::FAILURE
                : BT::NodeStatus::SUCCESS;
        });

    factory.registerNodeType<NavigateToPoseStatefulNode>("NavigateToPose");

    factory.registerSimpleAction(
        "Grasp",
        [context](BT::TreeNode&)
        {
            if(context && context->grasp)
                context->grasp();
            return BT::NodeStatus::SUCCESS;
        });

    factory.registerSimpleAction(
        "Place",
        [context](BT::TreeNode&)
        {
            if(context && context->place)
                context->place();
            return BT::NodeStatus::SUCCESS;
        });

    factory.registerSimpleAction(
        "Report",
        [context](BT::TreeNode&)
        {
            if(context && context->report)
                context->report(true);
            return BT::NodeStatus::SUCCESS;
        });

    factory.registerSimpleAction(
        "AbortAndReport",
        [context](BT::TreeNode&)
        {
            if(context && context->report)
                context->report(false);
            return BT::NodeStatus::FAILURE;
        });

    factory.registerSimpleAction(
        "ReleaseTrafficRight",
        [context](BT::TreeNode&)
        {
            if(context && context->release_traffic_right)
                context->release_traffic_right();
            return BT::NodeStatus::SUCCESS;
        });
}

}
