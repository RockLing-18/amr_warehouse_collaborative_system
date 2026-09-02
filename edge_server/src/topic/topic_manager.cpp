#include "topic/topic_manager.h"
#include "websocket/websocket_server.h"
#include "utils/LogDefine.h"


namespace edge_server
{
TopicManager::TopicManager(const std::shared_ptr<WebSocketServer>& websocketServer)
: m_websocketServer(websocketServer)
{
}

void TopicManager::subscribe(uint64_t clientId, const std::string& topic)
{
    LOG_INFO("clientId:{} subscribe topic:{}", clientId, topic);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_subscribers[topic].insert(clientId);
}

void TopicManager::unsubscribe(uint64_t clientId, const std::string& topic)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto iter = m_subscribers.find(topic);
    if(iter != m_subscribers.end())
    {
        iter->second.erase(clientId);
        LOG_INFO("clientId:{} unsubscribe topic:{}", clientId, topic);
    }
}

void TopicManager::publish(const std::string& topic, const std::string& message)
{
    std::vector<uint64_t> clients;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto iter = m_subscribers.find(topic);
        if(iter == m_subscribers.end())
            return;

        clients.assign(iter->second.begin(), iter->second.end());
    }

    for(auto id : clients)
    {
        if(m_websocketServer->sendToWSClient(id, message) == false)
        {
            unsubscribe(id, topic);
        }
        else
        {
            LOG_DEBUG("publish topic:{}  to clientId:{}", topic, id);
        }
    }
}

bool TopicManager::hasSubscriber(const std::string& topic)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto iter = m_subscribers.find(topic);
    return iter != m_subscribers.end() && !iter->second.empty();
}

}