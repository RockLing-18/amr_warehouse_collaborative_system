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
    using Handler = std::function<void(const std::string&)>;

    EdgeCommunication();
    ~EdgeCommunication();
    bool init(const BootstrapInfo& cfg, rclcpp::Node::SharedPtr node);

    bool publish(const std::string& topic, const std::string& payload, int qos = 1, bool retain = false);
    bool isConnected() const;
    void registerExternalHandler(const std::string& topic, Handler handler);
    void setExternalSubscribe(const std::string& topic, int qos = 1);
    
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
