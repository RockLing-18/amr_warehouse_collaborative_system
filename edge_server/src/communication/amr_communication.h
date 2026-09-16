#pragma once

#include <memory>
#include <string>
#include <functional>
#include "types.h"

namespace edge_server
{

class MqttClient;
class MqttMessageRouter;
class ServiceContext;

class AmrCommunication
{
public:
    AmrCommunication();
    bool init(const MqttCfg& cfg, const std::shared_ptr<ServiceContext>& serviceContext);
    
private:
    void setSubscribe();
    void registerHandler();
    void robotRegisterReqHandler(const std::string& msg);
    void robotStatusHandler(const std::string& msg);
    void robotWillHandler(const std::string& msg);

private:
    std::unique_ptr<MqttClient> m_mqtt;
    std::unique_ptr<MqttMessageRouter> m_router;
    std::shared_ptr<ServiceContext> m_context;
};


}