#include "edge_server/topic/topic_manager.h"
#include "edge_server/websocket/websocket_server.h"
#include <iostream>


namespace edge_server
{
TopicManager::TopicManager(const std::shared_ptr<WebSocketServer>& websocketServer)
: m_websocketServer(websocketServer)
{
}

void TopicManager::subscribe(uint64_t clientId, const std::string& topic)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_subscribers[topic].insert(clientId);
    std::cout << "clientId:" << clientId << " subscribe topic:" << topic << std::endl;
}

void TopicManager::unsubscribe(uint64_t clientId, const std::string& topic)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto iter = m_subscribers.find(topic);
    if(iter != m_subscribers.end())
    {
        iter->second.erase(clientId);
        std::cout << "clientId:" << clientId << " unsubscribe topic:" << topic << std::endl;
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
             std::cout << " publish topic:" << topic <<  " to clientId:" << id << std::endl;
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