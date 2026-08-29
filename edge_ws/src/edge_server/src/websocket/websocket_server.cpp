#include "edge_server/websocket/websocket_server.h"
#include "edge_server/websocket/websocket_session.h"
#include <libwebsockets.h>
#include <iostream>
#include <cstring>
#include <algorithm>

namespace edge_server
{

static bool sendToWSClientInstance(struct lws* wsi, const std::string& msg)
{
    std::vector<unsigned char> buffer(LWS_PRE + msg.size());
    memcpy(buffer.data() + LWS_PRE, msg.data(), msg.size());

    int ret = lws_write(wsi, buffer.data() + LWS_PRE, msg.size(), LWS_WRITE_TEXT);
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
        clientSessionId = server->addClientSession(wsi);
        std::cout << "client connected, id: " << clientSessionId << std::endl;
        break;
    }
    case LWS_CALLBACK_RECEIVE:
    {
        uint64_t& clientSessionId = *static_cast<uint64_t*>(user);
        std::string msg(static_cast<char*>(in), len);
        server->pushReceiveMessage(clientSessionId, msg);
        break;
    }
    case LWS_CALLBACK_CLOSED:
    {
        uint64_t& clientSessionId = *static_cast<uint64_t*>(user);
        server->removeClientSession(clientSessionId);
        std::cout << "client disconnect" << std::endl;
        break;
    }
    case LWS_CALLBACK_SERVER_WRITEABLE:
    {
        uint64_t& clientSessionId = *static_cast<uint64_t*>(user);
        auto msg = server->popSendMsg(clientSessionId);
        if(!msg.empty())
        {
            sendToWSClientInstance(wsi, msg);
            if(server->hasMoreSendMsg(clientSessionId))
            {
                lws_callback_on_writable(wsi);
            }
        }
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

bool WebSocketServer::start(const std::string &/*host*/, int port)
{
    if(m_running)
        return false;
    
    m_impl->protocols[0] =
     {
        "edge-protocol",
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
    m_impl->context = lws_create_context(&info);

    if(!m_impl->context)
    {
        std::cerr << "create websocket context failed" << std::endl;
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
    while(m_running && m_impl->context)
    {
        lws_service(m_impl->context, 100);
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
                    return  !m_running || !m_receiveQueue.empty();
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

uint64_t WebSocketServer::addClientSession(struct lws* wsi)
{
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    uint64_t id = m_idGenerator.generate();
    auto session = std::make_shared<WebSocketSession>(wsi, id);
    m_sessionById.emplace(id, session);
    return id;
}

void WebSocketServer::removeClientSession(uint64_t clientSessionId)
{
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    auto iter = m_sessionById.find(clientSessionId);
    if(iter == m_sessionById.end())
        return;

    iter->second->setWsiInvalid();
    m_sessionById.erase(iter);
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

}