#include "context/service_context.h"
#include "service/robot_service.h"
#include "service/bootstrap_service.h"
#include "service/map_service.h"


namespace edge_server
{
bool ServiceContext::init()
{
    m_robotService = std::make_shared<RobotService>();
    m_bootstrapService = std::make_shared<BootstrapService>();
    m_mapService = std::make_shared<MapService>();
    return true;
}

std::shared_ptr<RobotService> ServiceContext::getRobotService()
{
    return m_robotService;
}

std::shared_ptr<BootstrapService> ServiceContext::getBootstrapService()
{
    return m_bootstrapService;
}

std::shared_ptr<MapService> ServiceContext::getMapService()
{
    return m_mapService;
}


}