#include "mqtt/mqtt_message_router.h"
#include "utils/LogDefine.h"
#include "mqtt/mqtt_topic.h"
#include "mqtt/mqtt_client.h"
#include "robot/robot_manager.h"


namespace edge_server
{

MqttMessageRouter::MqttMessageRouter(const std::shared_ptr<RobotManager>& robotManager, const std::shared_ptr<MqttClient>& mqttClient)
: m_robot_manager(robotManager), m_mqtt_client(mqttClient)
{
    init();
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

void MqttMessageRouter::init()
{
    m_msgHandlers[mqtt_topic::ROBOT_REGISTER_REQ] =
        [this](const std::string& msg)
        {
            robotRegisterHandler(msg);
        };

    m_msgHandlers[mqtt_topic::MAP_REQUEST] =
        [this](const std::string& msg)
        {
            mapDataReqHandler(msg);
        };

    m_msgHandlers[mqtt_topic::TRAFFIC_RIGHTS_REQ] =
        [this](const std::string& msg)
        {
            robotRightHandler(msg);
        };
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

        messageParse(msg.topic, msg.payload);
    }
}

void MqttMessageRouter::messageParse(const std::string& topic, const std::string& message)
{
    auto iter = m_msgHandlers.find(topic);
    if(iter == m_msgHandlers.end())
    {
        LOG_WARN("mqtt unknown topic: {}", topic);
        return;
    }

    try
    {
        iter->second(message);
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("mqtt handler exception, topic={}, error={}", topic, e.what());
    }
    catch(...)
    {
        LOG_ERROR("mqtt handler unknown exception, topic={}", topic);
    }
}

void MqttMessageRouter::robotRegisterHandler(const std::string& message)
{
    // 1.解析
    // m_robot_manager->registerRobot()
}

void MqttMessageRouter::mapDataReqHandler(const std::string& message)
{}

void MqttMessageRouter::robotRightHandler(const std::string& message)
{}

}