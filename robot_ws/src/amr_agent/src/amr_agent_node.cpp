#include "amr_agent_node.h"
#include "types.h"
#include "bootstrap/bootstrap.h"

namespace amr_agent
{

AmrAgentNode::AmrAgentNode()
: Node("amr_agent")
{
    this->declare_parameter<std::string>("cfg_path","");
}


bool AmrAgentNode::init()
{
    std::string cfgPath;
    this->get_parameter("cfg_path", cfgPath);
    if(cfgPath.empty())
    {
        cfgPath = "/opt/amr/config/amr_config.yaml";
    }
    
    Bootstrap bootstrap;
    BootstrapInfo info;
    if(!bootstrap.run(get_logger(), cfgPath, info))
    {
        RCLCPP_ERROR(get_logger(), "bootstrap failed");
        return false;
    }

    // m_bootstrapInfo = info;
    // initMqtt(info.mqtt);
    // initMap(info);

    return true;
}

}