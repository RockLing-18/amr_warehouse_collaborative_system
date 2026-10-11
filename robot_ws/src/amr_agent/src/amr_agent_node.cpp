#include "amr_agent_node.h"
#include "types.h"
#include "bootstrap/bootstrap.h"
#include "communication/edge_communication.h"
#include "bridge/task_bridge.h"
#include "map/map_manager.h"
#include "utils/ros_logger.h"

namespace amr_agent
{

AmrAgentNode::AmrAgentNode()
: Node("amr_agent")
{
    this->declare_parameter<std::string>("cfg_path","");
}


bool AmrAgentNode::init()
{
    GlobalRosLogger::init(this->get_logger(), rclcpp::Logger::Level::Debug);

    std::string cfgPath;
    this->get_parameter("cfg_path", cfgPath);
    if(cfgPath.empty())
    {
        cfgPath = "/opt/amr/config/amr_config.yaml";
    }
    
    Bootstrap bootstrap;
    BootstrapInfo info;
    if(!bootstrap.run(cfgPath, info))
    {
        LOG_ERROR("bootstrap failed");
        return false;
    }

    m_communication = std::make_shared<EdgeCommunication>();
    if(!m_communication->init(info, shared_from_this()))
    {
        LOG_ERROR("edge communication init failed");
        return false;
    }

    m_mapManager = std::make_shared<MapManager>(shared_from_this());
    if(!m_mapManager->init(info.map.path))
    {
        LOG_ERROR("map manager init failed");
        return false;
    }

    m_taskBridge = std::make_shared<TaskBridge>(
        shared_from_this(),
        m_communication.get(),
        info.robot_id,
        info.warehouse_id);
    m_taskBridge->init();

    return true;
}

}
