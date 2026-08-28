#include "edge_server/websocket/websocket_session.h"

namespace edge_server
{

WebSocketSession::WebSocketSession(struct lws* wsi, uint64_t id)
: m_wsi(wsi), m_clientSessionId(id)
{
}

lws* WebSocketSession::getWsi() const
{
    return m_wsi.load();
}

void WebSocketSession::pushMessage(const std::string& message)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sendQueue.push(message);
}

bool WebSocketSession::isEmptyMessage()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sendQueue.empty();
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

void WebSocketSession::setWsiInvalid()
{
    m_wsi.store(nullptr);
}

bool WebSocketSession::isWsiValid()  const 
{
    return m_wsi.load() != nullptr; 
}

}