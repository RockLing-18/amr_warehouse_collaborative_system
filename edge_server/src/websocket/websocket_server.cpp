#include "websocket/websocket_server.h"
#include "websocket/websocket_session.h"
#include <libwebsockets.h>
#include <iostream>
#include <cstring>
#include <algorithm>
#include "utils/LogDefine.h"

namespace edge_server
{

static bool sendToWSClientInstance(struct lws* wsi, const std::string& msg)
{
    std::vector<unsigned char> buffer(LWS_PRE + msg.size());
    memcpy(buffer.data() + LWS_PRE, msg.data(), msg.size());

    int ret = lws_write(wsi, buffer.data() + LWS_PRE, msg.size(), LWS_WRITE_TEXT);
    return ret >= 0;
}

static bool sendPingToClient(struct lws* wsi)
{
    if(!wsi)
        return false;

    // 改进：不带payload ping
    // unsigned char buffer[LWS_PRE];
    // int ret = lws_write(wsi, buffer + LWS_PRE, 0, LWS_WRITE_PING);
    // return ret >= 0;

    // WebSocket Ping 控制帧。    
    unsigned char buffer[LWS_PRE + 1];
    const unsigned char pingData = 0x01;
    buffer[LWS_PRE] = pingData;
    int ret = lws_write(wsi, buffer + LWS_PRE, 1, LWS_WRITE_PING);
    return ret >= 0;
}

static int wsCallback(struct lws* wsi,  enum lws_callback_reasons reason, void* user, void* in,  size_t len)
{
    if(!wsi)
        return 0;
    
    auto* protocol = lws_get_protocol(wsi);
    if (!protocol)
        return 0;

    auto* server = static_cast<WebSocketServer*>(protocol->user);
    if (!server)
        return 0;

    switch(reason)
    {
    case LWS_CALLBACK_ESTABLISHED:
    {
        uint64_t& clientSessionId = *static_cast<uint64_t*>(user);
        clientSessionId = server->onConnected(wsi);
        break;
    }
    case LWS_CALLBACK_RECEIVE:
    {
        uint64_t& clientSessionId = *static_cast<uint64_t*>(user);
        std::string msg(static_cast<char*>(in), len);
        server->onReceive(clientSessionId, msg);
        break;
    }
    case LWS_CALLBACK_CLOSED:
    {
        uint64_t& clientSessionId = *static_cast<uint64_t*>(user);
        server->onDisconnected(clientSessionId);
        break;
    }
    case LWS_CALLBACK_SERVER_WRITEABLE:
    {
        uint64_t& clientSessionId = *static_cast<uint64_t*>(user);
        server->onWriteable(clientSessionId);
        break;
    }
    case LWS_CALLBACK_RECEIVE_PONG:
    {
        uint64_t& clientSessionId = *static_cast<uint64_t*>(user);
        server->onPong(clientSessionId);
        break;
    }
    default:
        // LWS_CALLBACK_PROTOCOL_INIT 等内部reason走到这里，完全不访问user
        break;
    }
    return 0;
}


struct WebSocketServer::Impl
{
    struct lws_protocols protocols[2];
    lws_context* context{nullptr};

    Impl()
    {
        memset(protocols, 0, sizeof(protocols));
    }

    ~Impl()
    {
    }
};

WebSocketServer::WebSocketServer()
{
    m_impl = std::make_unique<Impl>();
}

WebSocketServer::~WebSocketServer()
{
    stop();
}

bool WebSocketServer::start(const std::string &/*host*/, int port, const std::string& protocolName, const WebSocketServerOptions& options)
{
    if(m_running)
        return false;
    
    m_impl->protocols[0] =
     {
        protocolName.c_str(),
        wsCallback,
        sizeof(uint64_t), // 存客户端 ID
        4096,
        0,
        this,
        0
    };

    lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    
    info.port = port;
    info.protocols = m_impl->protocols;
    m_options = options;
    m_impl->context = lws_create_context(&info);

    if(!m_impl->context)
    {
        LOG_ERROR("create websocket context failed");
        return false;
    }

    m_running = true;
    m_messageThread = std::thread(&WebSocketServer::messageThread, this);
    m_thread = std::thread(&WebSocketServer::serviceThread, this);
    
    return true;
}

void WebSocketServer::stop()
{
    m_running = false;
    if(m_impl->context)
    {
        lws_cancel_service(m_impl->context);
    }

    if(m_thread.joinable())
        m_thread.join();

    m_receiveCv.notify_all();
    if(m_messageThread.joinable())
        m_messageThread.join();

    if(m_impl->context)
    {
        lws_context_destroy(m_impl->context);
        m_impl->context = nullptr;
    }

    std::lock_guard<std::mutex> lock(m_sessionMutex);
    m_sessionById.clear();
}

bool WebSocketServer::sendToWSClient(uint64_t clientSessionId, const std::string& message)
{
    std::shared_ptr<WebSocketSession> session = pushSendMsg(clientSessionId, message);
    if (!session) 
        return false;

    struct lws* wsi = session->getWsi();  // 原子读取
    if (wsi)
    {
        triggerWritable(wsi);
        return true;
    }
        
    return false;
}

void WebSocketServer::triggerWritable(struct lws *wsi)
{
    lws_callback_on_writable(wsi);
    if(m_impl->context)
        lws_cancel_service(m_impl->context);
}

void WebSocketServer::setMessageCallback(MessageCallback callback)
{
    m_message_callback = callback;
}

void WebSocketServer::serviceThread()
{
    auto lastHeartbeatCheck = std::chrono::steady_clock::now(); 
    
    while(m_running && m_impl->context) 
    { 
        lws_service( m_impl->context, m_options.serviceTimeoutMs); 
        auto now = std::chrono::steady_clock::now(); 
        // 心跳检查1秒检查一次足够。 
        if(now - lastHeartbeatCheck >= std::chrono::seconds(5)) 
        { 
            checkHeartbeat(); 
            lastHeartbeatCheck = now; 
        } 
    }
}

void WebSocketServer::messageThread()
{
    while(m_running)
    {
        WebSocketMessage message;

        {
            std::unique_lock<std::mutex> lock(m_receiveMutex);
            m_receiveCv.wait(
                lock,
                [this]
                {
                    return !m_running || !m_receiveQueue.empty();
                });

            if(!m_running)
                break;

            if(m_receiveQueue.empty())
                continue;

            message = m_receiveQueue.front();
            m_receiveQueue.pop();
        }

        if(m_message_callback)
        {
            m_message_callback(message.client, message.data);
        }
    }
}

std::shared_ptr<WebSocketSession> WebSocketServer::addClientSession(struct lws* wsi)
{
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    uint64_t id = m_idGenerator.generate();
    auto session = std::make_shared<WebSocketSession>(wsi, id);
    m_sessionById.emplace(id, session);
    return session;
}

std::shared_ptr<WebSocketSession> WebSocketServer::removeClientSession(uint64_t clientSessionId)
{
    std::shared_ptr<WebSocketSession> session;
    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        auto iter = m_sessionById.find(clientSessionId);
        if(iter == m_sessionById.end())
            return session;

        session = iter->second;
        m_sessionById.erase(iter);
    }
    
    return session;
}

std::shared_ptr<WebSocketSession> WebSocketServer::getClientSession(uint64_t clientSessionId)
{
    std::shared_ptr<WebSocketSession> session;
    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        auto iter = m_sessionById.find(clientSessionId);
        if(iter != m_sessionById.end())
            session = iter->second;
    }
   
    return session;
}

void WebSocketServer::pushReceiveMessage(uint64_t clientSessionId, const std::string &message)
{
    std::unique_lock<std::mutex> lock(m_receiveMutex);
    WebSocketMessage msg;
    msg.client = clientSessionId;
    msg.data = message;
    m_receiveQueue.push(msg);
    m_receiveCv.notify_one();
}

std::shared_ptr<WebSocketSession> WebSocketServer::pushSendMsg(uint64_t clientSessionId, const std::string& message)
{
    std::shared_ptr<WebSocketSession> session;
    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        if(m_sessionById.find(clientSessionId) != m_sessionById.end())
            session = m_sessionById.at(clientSessionId);
    }

    if(session)
        session->pushMessage(message);

    return session;
}

std::string WebSocketServer::popSendMsg(uint64_t clientSessionId)
{
    std::string msg;
    std::shared_ptr<WebSocketSession> session;
    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        if(m_sessionById.find(clientSessionId) == m_sessionById.end())
            return msg;

        session = m_sessionById.at(clientSessionId);
    }

    msg = session->popMessage();
    return msg;
}

bool WebSocketServer::hasMoreSendMsg(uint64_t clientSessionId)
{
    std::shared_ptr<WebSocketSession> session;
    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        auto it = m_sessionById.find(clientSessionId);
        if(it == m_sessionById.end())
            return false;

        session = m_sessionById.at(clientSessionId);
    }

    return !session->isEmptyMessage();
}

void WebSocketServer::checkHeartbeat()
{
    const auto now = std::chrono::steady_clock::now();
    std::vector<uint64_t> timeoutClients;
    std::vector<lws*> needWritableWsi;

    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        for(const auto& [clientId, session] : m_sessionById)
        {
            if(!session || !session->isWsiValid())
                continue;

            // 已经发送Ping，但是没有收到Pong
            if(session->isPingOutstanding())
            {
                const auto elapsed = now - session->getLastPingTime();
                if(elapsed >= m_options.heartbeatTimeout)
                {
                    timeoutClients.push_back(clientId);
                }

                continue;
            }

            /*
             * 没有Ping等待。
             *
             * 如果距离上一次Pong已经超过heartbeatInterval，
             * 则申请WRITEABLE，让LWS发送Ping。
             */
            const auto elapsed = now - session->getLastPongTime();
            if(elapsed >= m_options.heartbeatInterval)
            {
                lws* wsi = session->getWsi();
                if(wsi)
                {
                    needWritableWsi.push_back(wsi);
                }
            }
        }
    }

    for(lws* wsi : needWritableWsi)
    {
        lws_callback_on_writable(wsi);
    }

    /*
     * 心跳超时的客户端统一清理
     */
    for(uint64_t clientId : timeoutClients)
    {
        LOG_WARN("[WebSocketServer] heartbeat timeout, client={}", clientId);
        cleanupSession(clientId);
    }
}


void WebSocketServer::cleanupSession(uint64_t clientSessionId)
{
    auto session = removeClientSession(clientSessionId);
    if(!session)
        return;

    lws* wsi = session->getWsi();
    session->setWsiInvalid();
    if(wsi)
    {
        /*
         * 当前仍然处于LWS service线程，
         * 可以安全关闭。
         */
        lws_set_timeout(wsi, PENDING_TIMEOUT_CLOSE_SEND, 1);
    }
}

uint64_t WebSocketServer::onConnected(struct lws *wsi)
{
    auto session = addClientSession(wsi);
    char peerIp[INET6_ADDRSTRLEN] = {0};
    lws_get_peer_simple(wsi, peerIp, sizeof(peerIp));
    std::string clientIp = stripIpv4MappedPrefix(peerIp);
    session->setClientIp(clientIp);
    LOG_INFO("client connected, clientIp:{} clientSessionId={}", clientIp, session->getClientId());
    return session->getClientId();
}

void WebSocketServer::onDisconnected(uint64_t clientSessionId)
{
    auto session = removeClientSession(clientSessionId);
    if(!session)
        return;

    session->setWsiInvalid();
    LOG_INFO("client disconnect, clientIp:{} clientSessionId={}", session->getClientIp(), clientSessionId);
}

void WebSocketServer::onWriteable(uint64_t clientSessionId)
{
    auto session = getClientSession(clientSessionId); 
    if(!session) 
        return; 

    //1. Ping优先
    auto now = std::chrono::steady_clock::now(); 
    if(!session->isPingOutstanding() && now - session->getLastPongTime() >= m_options.heartbeatInterval) 
    { 
        if(sendPingToClient(session->getWsi())) 
        { 
            session->markPingSent(); 
            LOG_INFO("[WebSocketServer] ping clientSessionId={}", clientSessionId);
        } 

        // Ping之后继续处理业务消息 
        if(hasMoreSendMsg(clientSessionId)) 
        { 
            lws_callback_on_writable(session->getWsi()); 
        } 
        
        return; 
    } 
    
    // 2. 业务消息
    auto msg = popSendMsg(clientSessionId); 
    if(!msg.empty()) 
    { 
        if(!sendToWSClientInstance(session->getWsi(), msg)) 
        { 
            LOG_ERROR("[WebSocketServer] send message failed, clientSessionId={}", clientSessionId);
        } 
    } 
    
    // 3. 还有业务消息，继续触发WRITEABLE 
    if(hasMoreSendMsg(clientSessionId)) 
    { 
        lws_callback_on_writable(session->getWsi());
    }
}

void WebSocketServer::onReceive(uint64_t clientSessionId, const std::string& message)
{
    LOG_DEBUG("receive: {}", message);
    auto session = getClientSession(clientSessionId);
    if(session)
    {
        session->updatePong(); // 收到业务消息，重置心跳
    }

    pushReceiveMessage(clientSessionId, message);
}

void WebSocketServer::onPong(uint64_t clientSessionId)
{
    std::cout << " onPong .... " << std::endl;
    auto session = getClientSession(clientSessionId);
    if(session)
    {
        session->updatePong();
        LOG_INFO("[WebSocketServer] client response pong, clientSessionId: {}", clientSessionId);
    }
}

}