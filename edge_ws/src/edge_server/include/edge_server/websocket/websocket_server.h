#pragma once
#include <string>

namespace edge_server
{

class WebSocketServer
{
public:
    WebSocketServer();
    ~WebSocketServer();

    bool start(const std::string& host, int port);
    void stop();


    void broadcast(const std::string& message);


};


}