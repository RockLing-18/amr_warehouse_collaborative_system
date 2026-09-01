#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <memory>

namespace edge_server
{

class WebSocketServer;

class TopicManager
{
public:
    TopicManager(const std::shared_ptr<WebSocketServer>& websocketServer);

    void subscribe(uint64_t clientId, const std::string& topic);
    void unsubscribe(uint64_t clientId,  const std::string& topic);
    void publish(const std::string& topic, const std::string& message);
    bool hasSubscriber(const std::string& topic);

private:
    std::unordered_map<std::string, std::unordered_set<uint64_t>> m_subscribers;
    std::mutex m_mutex;

    std::shared_ptr<WebSocketServer> m_websocketServer;
};

}