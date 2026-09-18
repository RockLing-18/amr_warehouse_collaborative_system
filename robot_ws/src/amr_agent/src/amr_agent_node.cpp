#include "amr_agent_node.h"
#include "types.h"
#include "bootstrap/bootstrap.h"

namespace amr_agent
{

AmrAgentNode::AmrAgentNode()
: Node("amr_agent")
{

}


void AmrAgentNode::init()
{
    std::string cfgPath = "";
    Bootstrap bootstrap;
    BootstrapInfo info;
    if(!bootstrap.run(get_logger(), cfgPath, info))
    {
        RCLCPP_ERROR(get_logger(), "bootstrap failed");
        return;
    }

    // m_bootstrapInfo = info;
    // initMqtt(info.mqtt);
    // initMap(info);

}

}