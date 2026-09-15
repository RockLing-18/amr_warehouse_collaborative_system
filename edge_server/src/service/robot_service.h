#pragma once

#include <memory>
#include <string>
#include "types.h"

namespace edge_server
{

class RobotManager;
class MqttClient;
class MapService;


class RobotService
{
public:
    RobotService(const std::shared_ptr<RobotManager>& robotManager, const std::shared_ptr<MqttClient>& edgeAmrMqttClient, const std::shared_ptr<MapService>& mapService);
    void handleRegister(const std::string& message);
    void handleStatus(const std::string& message);
    void handleWill(const std::string& message);

private:
    void sendRegisterResponse(const std::string& robotId, const std::string& requestId, MapUpdateInfo& info);

private:
    std::shared_ptr<RobotManager> m_robotManager;
    std::shared_ptr<MqttClient> m_edgeAmrMqttClient;
    std::shared_ptr<MapService> m_mapService;
};

}