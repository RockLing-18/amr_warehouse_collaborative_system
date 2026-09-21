#include "mqtt/mqtt_message_router.h"
#include "mqtt/mqtt_topic.h"
#include "mqtt/mqtt_client.h"
#include "nlohmann/json.hpp"
#include "utils/ros_logger.h"

using json = nlohmann::json;

namespace amr_agent
{

MqttMessageRouter::MqttMessageRouter()
{
    m_running = true;
    m_messageThread = std::thread(&MqttMessageRouter::messageConsumerThread, this);
}

MqttMessageRouter::~MqttMessageRouter()
{
    m_running = false;
    m_cv.notify_all();

    if(m_messageThread.joinable())
        m_messageThread.join();
}

void MqttMessageRouter::registerHandler(const std::string& topic, Handler handler)
{
    if(!handler)
    {
        LOG_ERROR("register mqtt handler failed topic=%s", topic.c_str());
        return;
    }

    m_msgHandlers[topic] = std::move(handler);
    LOG_INFO("register mqtt handler topic=%s", topic.c_str());
}

void MqttMessageRouter::onMessageProducer(const std::string& topic, const std::string& message)
{
    LOG_DEBUG("mqtt recv topic=%s, msg=%s", topic.c_str(), message.c_str());
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        MqttMessage msg;
        msg.topic = topic;
        msg.payload = message;
        m_msgQueue.push(msg);
    }

    m_cv.notify_one();
}

void MqttMessageRouter::messageConsumerThread()
{
    while(m_running)
    {
       MqttMessage msg;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(
                lock,
                [this]
                {
                    return !m_running || !m_msgQueue.empty();
                });

            if(!m_running)
                break;

            if(m_msgQueue.empty())
                continue;

            msg = std::move(m_msgQueue.front());
            m_msgQueue.pop();
        }

        messageRouter(msg.topic, msg.payload);
    }
}

void MqttMessageRouter::messageRouter(const std::string& topic, const std::string& message)
{
    auto iter = m_msgHandlers.find(topic);
    if(iter == m_msgHandlers.end())
    {
        LOG_WARN("unknown mqtt topic: %s", topic.c_str());
        return;
    }

    iter->second(message);
}

}