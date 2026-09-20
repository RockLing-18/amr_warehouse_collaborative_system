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
#include "types.h"

namespace edge_server
{

class MqttMessageRouter
{
public:
    using Handler = std::function<void(const std::string&)>;
public:
    MqttMessageRouter();
    ~MqttMessageRouter();
    void onMessageProducer(const std::string& topic, const std::string& message);
    void registerHandler(const std::string& topic, Handler handler);

private:
    void messageConsumerThread();
    void messageRouter(const std::string& topic, const std::string& message);

private:
    std::atomic<bool> m_running{false};
    std::thread m_messageThread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<MqttMessage> m_msgQueue;
    std::unordered_map<std::string, Handler> m_msgHandlers;
};

}