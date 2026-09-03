#include "mqtt/mqtt_message_router.h"
#include "utils/LogDefine.h"


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

void MqttMessageRouter::onMessageProducer(const std::string& topic, const std::string& message)
{
    LOG_INFO("mqtt recv topic={}, msg={}", topic, message);
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_msgQueue.push(std::pair(topic, message));
    }

    m_cv.notify_one();
}

void MqttMessageRouter::messageConsumerThread()
{
    while(m_running)
    {
       std::pair<std::string, std::string> msg;

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

        messageParse(msg.first, msg.second);
    }
}

void MqttMessageRouter::messageParse(const std::string& topic, const std::string& message)
{
    if (topic == "amr/register/request")
    {
        robotRegisterHandler(message);
    }
    else if (topic == "warehouse/map_data/request/")
    {
        mapDataReqHandler(message);
    }
    else if (topic == "amr/robot_right/request/")
    {
        robotRightHandler(message);
    }
}

void MqttMessageRouter::robotRegisterHandler(const std::string& message)
{}

void MqttMessageRouter::mapDataReqHandler(const std::string& message)
{}

void MqttMessageRouter::robotRightHandler(const std::string& message)
{}

}