#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

namespace edge_server
{


class MqttMessageRouter
{
public:
    MqttMessageRouter();
    ~MqttMessageRouter();
    void onMessageProducer(const std::string& topic, const std::string& message);

private:
    void messageConsumerThread();
    void messageParse(const std::string& topic, const std::string& message);
    void robotRegisterHandler(const std::string& message);
    void mapDataReqHandler(const std::string& message);
    void robotRightHandler(const std::string& message);

private:
    std::atomic<bool> m_running{false};
    std::thread m_messageThread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<std::pair<std::string, std::string>> m_msgQueue;
};

}