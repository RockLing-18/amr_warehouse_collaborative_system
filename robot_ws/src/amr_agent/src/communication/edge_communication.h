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

    using RegisterResponseCallback = std::function<void(const RobotRegisterResponse&)>;
    void setRegisterResponseCallback(RegisterResponseCallback callback);
private:
    RegisterResponseCallback m_registerResponseCallback;
    

//     using TrafficResponseCallback = std::function<void(const TrafficResponse&)>;
//     void setTrafficResponseCallback(TrafficResponseCallback callback);
// private:
//     TrafficResponseCallback m_trafficResponseCallback;

private:
    std::string m_robotId;
    std::unique_ptr<MqttClient> m_mqtt;
    std::unique_ptr<MqttMessageRouter> m_router;
};


}