#include "edge_server/websocket/websocket_message_router.h"
#include "edge_server/common/json.hpp"
#include <iostream>

namespace edge_server
{

using json = nlohmann::json;

void WebSocketMessageRouter::onMessage(lws* client, const std::string& message)
{
    try
    {
        auto j = json::parse(message);
        if(!j.contains("type"))
            return;

        auto type = j["type"];
        if(type == "subscribe")
        {
            handleSubscribe(client, message);
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what();
    }
}


void WebSocketMessageRouter::handleSubscribe(lws* client, const std::string& message)
{
    try
    {
        auto j = json::parse(message);
        for(auto& topic : j["topics"])
        {
            //m_topic_manager->subscribe(topic, client);
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what();
    }
}


}