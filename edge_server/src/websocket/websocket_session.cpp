#include "websocket/websocket_session.h"

namespace edge_server
{

WebSocketSession::WebSocketSession(struct lws* wsi, uint64_t id)
: m_wsi(wsi), m_clientSessionId(id)
{
    const auto now = Clock::now(); 
    m_lastPongTime.store(now);
    m_lastPingTime.store(now);
}

uint64_t WebSocketSession::getClientId() const
{
    return m_clientSessionId;
}

lws* WebSocketSession::getWsi() const
{
    return m_wsi.load();
}

void WebSocketSession::setClientIp(const std::string& ip)
{
    m_clientIp = ip;
}

std::string WebSocketSession::getClientIp() const
{
    return m_clientIp;
}

bool WebSocketSession::pushMessage(const std::string& message)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(m_sendQueue.size() >= MAX_SEND_QUEUE)
        return false;
    
    m_sendQueue.push(message);
    return true;
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
            message = std::move(m_sendQueue.front());
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

void WebSocketSession::updatePong() 
{ 
    m_lastPongTime.store(Clock::now()); 
    m_pingOutstanding.store(false); 
} 

void WebSocketSession::markPingSent() 
{ 
    m_lastPingTime.store(Clock::now()); 
    m_pingOutstanding.store(true); 
}

bool WebSocketSession::isPingOutstanding() const
{ 
    return m_pingOutstanding.load(); 
}

WebSocketSession::Clock::time_point WebSocketSession::getLastPongTime() const 
{ 
    return m_lastPongTime.load(); 
} 

WebSocketSession::Clock::time_point WebSocketSession::getLastPingTime() const 
{ 
    return m_lastPingTime.load(); 
}

}