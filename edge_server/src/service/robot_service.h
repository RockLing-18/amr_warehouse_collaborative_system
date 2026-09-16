#pragma once

#include <memory>
#include <string>
#include "types.h"

namespace edge_server
{

class RobotManager;
class MqttClient;
class MapService;

struct RobotServiceRuntime
{
    std::shared_ptr<RobotManager> robotManager;
    std::shared_ptr<MapService> mapService;
};

class RobotService
{
public:
    RobotService();
    void init(const RobotServiceRuntime& info);
    bool handleRegister(const RobotRegisterRequest& req, RobotRegisterResponse& resp);
    void handleStatus(const RobotRunningStatus& status);
    void handleWill(const std::string& robotId);

private:
    std::shared_ptr<RobotManager> m_robotManager;
    std::shared_ptr<MapService> m_mapService;
};

}