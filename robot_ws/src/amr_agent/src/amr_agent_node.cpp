#include "amr_agent_node.h"

namespace amr_agent
{

AmrAgentNode::AmrAgentNode()
: Node("amr_agent")
{

}


bool AmrAgentNode::init()
{

    Bootstrap bootstrap;


    BootstrapInfo info;


    if(!bootstrap.run(info))
    {
        RCLCPP_ERROR(
            get_logger(),
            "bootstrap failed");

        return false;
    }


    m_bootstrapInfo = info;


    initMqtt(info.mqtt);


    initMap(info);


    return true;
}

}