#include "mqtt/mqtt_client.h"
#include "utils/LogDefine.h"
#include "utils/CommonFunc.h"
#include "spdlog/fmt/fmt.h"
#include "mqtt/mqtt_topic.h"

namespace edge_server
{

MqttClient::MqttClient()
{

}

MqttClient::~MqttClient()
{
    // disconnect();
}


bool MqttClient::init(const MqttCfg& cfg)
{
    try
    {
        m_client = std::make_unique<mqtt::async_client>(cfg.url, cfg.client_id);
        m_client->set_callback(*this);
        m_options.set_mqtt_version(MQTTVERSION_3_1_1);
        m_options.set_user_name(cfg.user);
        m_options.set_password(cfg.pwd);
        m_options.set_keep_alive_interval(cfg.keepalive);
        m_options.set_automatic_reconnect(1, cfg.reconnect_interval);
        m_options.set_clean_session(false);
        m_url = cfg.url;
        m_willEnable = cfg.will_msg_enable;
        if(cfg.will_msg_enable)
        {
            mqtt::will_options will(
                cfg.will.topic,
                cfg.will.payload,
                1,
                true
                );

            m_options.set_will(will);
            m_will = cfg.will;
        }

        // ========= TLS 配置 =========
        if(cfg.tls_enable)
        {
            mqtt::ssl_options sslOpt;

            // 单向TLS：设置CA根证书，校验服务端证书
            sslOpt.set_trust_store(cfg.ca_file);

            // 调试用：关闭证书校验
            // sslOpt.set_verify(false);

            // 将tls配置挂载到connect选项
            m_options.set_ssl(sslOpt);
        }

        return true;
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("mqtt connect failed:{}", e.what());
        return false;
    }
}

bool MqttClient::connect()
{
    if(!m_client)
    {
        LOG_ERROR("mqtt client not init");
        return false;
    }

    if(m_client->is_connected())
    {
        LOG_INFO("mqtt already connected");
        return true;
    }

    try
    {
        auto token = m_client->connect(m_options);
        // 设置连接超时，5秒
        if(!token->wait_for(std::chrono::seconds(5)))
        {
            LOG_ERROR("mqtt connect timeout");
            return false;
        }

        if(token->get_return_code() != MQTTASYNC_SUCCESS)
        {
            LOG_ERROR("mqtt connect code={}", token->get_return_code());
            return false;
        }

        LOG_INFO("mqtt connect ok:{}", m_url);
        return true;
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("mqtt connect failed:{}", e.what());
        return false;
    }
}

void MqttClient::disconnect()
{
    if(m_client)
    {
        try
        {
            auto token = m_client->disconnect();
            // 设置断开连接超时，5秒
            if(!token->wait_for(std::chrono::seconds(5)))
            {
                LOG_ERROR("mqtt disconnect timeout");
            }
        }
        catch(...)
        {
        }
    }
}

bool MqttClient::subscribe(const std::string& topic, int qos)
{
    try
    {
        m_client->subscribe(topic, qos);

        LOG_INFO("mqtt subscribe:{}", topic);
        return true;
    }
    catch(...)
    {
        return false;
    }
}

bool MqttClient::publish(const std::string& topic, const std::string& payload, int qos, bool retain)
{
    try
    {
        auto msg = mqtt::make_message(topic, payload);
        msg->set_qos(qos);
        msg->set_retained(retain);
        m_client->publish(msg);
        return true;
    }
    catch(...)
    {
        return false;
    }
}

void MqttClient::setMessageCallback(MessageCallback cb)
{
    m_callback = cb;
}

// 新增回调：连接成功
void MqttClient::connected(const std::string& cause)
{
    LOG_INFO("mqtt connected success, cause:{}", cause);

    if(m_willEnable)
    {
        std::string sPayload = R"({{"status":"online","timestamp":{}}})";
        sPayload = fmt::format(sPayload, utils::getCurrentTimeMs());
        publish(m_will.topic, sPayload, 1, true);
    }

    // 恢复订阅
    subscribe(mqtt_topic::ROBOT_REGISTER_REQ, 1);
}

void MqttClient::message_arrived(mqtt::const_message_ptr msg)
{
    if(m_callback)
    {
        m_callback(msg->get_topic(), msg->to_string());
    }
}

void MqttClient::delivery_complete(mqtt::delivery_token_ptr tok)
{
    if(tok)
    {
        LOG_DEBUG("mqtt message delivered id={}", tok->get_message_id());
    }
}

void MqttClient::connection_lost(const std::string& cause)
{
    LOG_WARN("mqtt connection lost:{}", cause);
}

}