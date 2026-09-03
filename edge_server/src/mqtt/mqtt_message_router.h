#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <memory>

namespace edge_server
{

struct MqttMessage
{
    std::string topic;
    std::string payload;
};

class RobotManager;
class MqttClient;

class MqttMessageRouter
{
public:
    using Handler = std::function<void(const std::string&)>;
public:
    MqttMessageRouter(const std::shared_ptr<RobotManager>& robotManager, const std::shared_ptr<MqttClient>& mqttClient);
    ~MqttMessageRouter();
    void onMessageProducer(const std::string& topic, const std::string& message);

private:
    void init(); 
    void messageConsumerThread();
    void messageParse(const std::string& topic, const std::string& message);
    void robotRegisterHandler(const std::string& messam_topicManagerge);
    void mapDataReqHandler(const std::string& message);
    void robotRightHandler(const std::string& message);

private:
    std::atomic<bool> m_running{false};
    std::thread m_messageThread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<MqttMessage> m_msgQueue;
    std::unordered_map<std::string, Handler> m_msgHandlers;
    std::shared_ptr<RobotManager> m_robot_manager;
    std::shared_ptr<MqttClient> m_mqtt_client;
};

}