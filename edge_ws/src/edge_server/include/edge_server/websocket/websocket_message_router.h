#pragma once

#include <string>

struct lws;

namespace edge_server
{

class WebSocketMessageRouter
{

public:
    void onMessage(lws* client, const std::string& msg);

private:
    void handleSubscribe(lws* client, json& msg);
};

}