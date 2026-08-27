#include "edge_server/websocket/websocket_session.h"
#include "edge_server/websocket/websocket_server.h"

namespace edge_server
{

IdGeneratorU64_t WebSocketSession::sm_idGenerator;

WebSocketSession::WebSocketSession(struct lws* wsi)
: m_wsi(wsi), m_clientSessionId(0)
{
    m_clientSessionId = sm_idGenerator.generate();
}

uint64_t WebSocketSession::getClientSessionId() const
{
    return m_clientSessionId;
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