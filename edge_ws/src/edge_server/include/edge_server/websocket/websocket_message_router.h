#pragma once

#include <string>

struct lws;

namespace edge_server
{

class WebSocketMessageRouter
{

public:
    void onMessage(lws* client, const std::string& message);

private:
    void handleSubscribe(lws* client, const std::string& message);

// private:
//     std::shared_ptr<TopicManager> m_topic_manager;
};

}