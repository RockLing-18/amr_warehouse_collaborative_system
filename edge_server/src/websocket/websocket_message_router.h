#pragma once

#include <memory>
#include <string>

namespace edge_server
{

class TopicManager;

class WebSocketMessageRouter
{
public:
    WebSocketMessageRouter(const std::shared_ptr<TopicManager>& topicManager);
    void onMessage(uint64_t clientId, const std::string& message);

private:
    void handleSubscribe(uint64_t clientId, const std::string& message);

private:
    std::shared_ptr<TopicManager> m_topic_manager;
};

}