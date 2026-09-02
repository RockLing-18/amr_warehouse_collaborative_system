#include "websocket/websocket_message_router.h"
#include "topic/topic_manager.h"
#include "nlohmann/json.hpp"
#include "utils/LogDefine.h"

namespace edge_server
{

using json = nlohmann::json;

 WebSocketMessageRouter::WebSocketMessageRouter(const std::shared_ptr<TopicManager>& topicManager)
 : m_topic_manager(topicManager)
 {
 }

void WebSocketMessageRouter::onMessage(uint64_t clientId, const std::string& message)
{
    try
    {
        auto j = json::parse(message);
        if(!j.contains("msgType"))
            return;

        auto type = j["msgType"];
        if(type == "subscribe")
        {
            handleSubscribe(clientId, message);
        }
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("exception: ", e.what());
    }
}


void WebSocketMessageRouter::handleSubscribe(uint64_t clientId, const std::string& message)
{
    try
    {
        auto j = json::parse(message);
        for(auto& topic : j["topics"])
        {
            m_topic_manager->subscribe(clientId, topic.get<std::string>());
        }
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("exception: ", e.what());
    }
}


}