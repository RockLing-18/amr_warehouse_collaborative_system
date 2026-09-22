#pragma once

#include <memory>
#include <string>
#include <functional>
#include "types.h"
#include <rclcpp/rclcpp.hpp>

namespace amr_agent
{

class MqttClient;
class MqttMessageRouter;
class ServiceContext;

class EdgeCommunication
{
public:
    EdgeCommunication();
    bool init(const BootstrapInfo& cfg, const std::shared_ptr<ServiceContext>& serviceContext);
    
private:
    void setSubscribe();
    void registerHandler();
    void edgeSvrStatusHandler(const std::string& msg);
    void robotRegisterRespHandler(const std::string& msg);
    
    // void robotWillHandler(const std::string& msg);

private:
    std::string m_robotId;
    std::unique_ptr<MqttClient> m_mqtt;
    std::unique_ptr<MqttMessageRouter> m_router;
    std::shared_ptr<ServiceContext> m_context;
};


}