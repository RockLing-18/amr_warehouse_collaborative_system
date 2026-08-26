#include "edge_server/websocket/websocket_server.h"


namespace edge_server
{
WebSocketServer::WebSocketServer()
{
}

WebSocketServer::~WebSocketServer()
{
}

bool WebSocketServer::start(const std::string &host, int port)
{
    return false;
}

void WebSocketServer::stop()
{
}

void WebSocketServer::broadcast(const std::string &message)
{
}
}