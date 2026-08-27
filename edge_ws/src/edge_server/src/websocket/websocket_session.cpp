#include "edge_server/websocket/websocket_session.h"
#include "edge_server/websocket/websocket_server.h"

namespace edge_server
{

WebSocketSession::WebSocketSession(struct lws* wsi)
: m_wsi(wsi)
{}

lws* WebSocketSession::getWsi() const
{
    return m_wsi;
}

void WebSocketSession::sendToClient(const std::string& message)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sendQueue.push(message);
    }
    
    // 触发发送信号
    WebSocketServer::sendToWSClient(m_wsi);
}

std::string WebSocketSession::popMessage()
{
    std::string message;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if(!m_sendQueue.empty())
        {
            message = m_sendQueue.front();
            m_sendQueue.pop();
        }
    }

    return message;
}

}