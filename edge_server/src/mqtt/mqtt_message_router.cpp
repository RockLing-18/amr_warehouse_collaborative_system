#include "mqtt/mqtt_message_router.h"
#include "utils/LogDefine.h"
#include <vector>
#include "mqtt/mqtt_topic.h"
#include "mqtt/mqtt_client.h"
#include "robot/robot_manager.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace edge_server
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
        LOG_ERROR("register mqtt handler failed topic={}", topic);
        return;
    }

    m_msgHandlers[topic] = std::move(handler);
    LOG_INFO("register mqtt handler topic={}", topic);
}

void MqttMessageRouter::onMessageProducer(const std::string& topic, const std::string& message)
{
    LOG_INFO("mqtt recv topic={}, msg={}", topic, message);
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
    if(iter != m_msgHandlers.end())
    {
        iter->second(message);
        return;
    }

    for(const auto& item : m_msgHandlers)
    {
        if(topicMatches(item.first, topic))
        {
            item.second(message);
            return;
        }
    }

    LOG_WARN("unknown mqtt topic {}", topic);
}

bool MqttMessageRouter::topicMatches(const std::string& filter, const std::string& topic)
{
    auto split = [](const std::string& input, std::vector<std::string>& levels)
    {
        std::size_t start = 0;
        while(start <= input.size())
        {
            std::size_t pos = input.find('/', start);
            if(pos == std::string::npos)
            {
                levels.push_back(input.substr(start));
                break;
            }

            levels.push_back(input.substr(start, pos - start));
            start = pos + 1;
        }
    };

    std::vector<std::string> filterLevels;
    std::vector<std::string> topicLevels;
    split(filter, filterLevels);
    split(topic, topicLevels);

    for(std::size_t i = 0; i < filterLevels.size(); ++i)
    {
        const std::string& level = filterLevels[i];
        if(level == "#")
        {
            return i == filterLevels.size() - 1;
        }

        if(i >= topicLevels.size())
            return false;

        if(level != "+" && level != topicLevels[i])
            return false;
    }

    return filterLevels.size() == topicLevels.size();
}

}