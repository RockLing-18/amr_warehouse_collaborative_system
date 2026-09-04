#pragma once

#include <mqtt/async_client.h>
#include <functional>
#include <memory>
#include <string>
#include "config/config.h"

namespace edge_server
{

class MqttClient : public virtual mqtt::callback
{
public:
    using MessageCallback = std::function<void(const std::string& topic, const std::string& payload)>;

public:
    MqttClient();
    ~MqttClient();

    bool init(const MqttCfg& cfg);
    bool connect();
    void disconnect();
    bool subscribe(const std::string& topic, int qos = 1);
    bool publish(const std::string& topic, const std::string& payload, int qos = 1, bool retain = false);
    void setMessageCallback(MessageCallback cb);

private:
    void connected(const std::string& cause) override;
    void message_arrived(mqtt::const_message_ptr msg) override;
    void delivery_complete(mqtt::delivery_token_ptr tok) override;
    void connection_lost(const std::string& cause) override;

private:
    std::unique_ptr<mqtt::async_client> m_client;
    mqtt::connect_options m_options;
    MessageCallback m_callback;
    std::string m_url;
    MqttMessage m_will;
    bool m_willEnable{false};
};

}