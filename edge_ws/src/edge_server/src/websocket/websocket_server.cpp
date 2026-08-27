#include "edge_server/websocket/websocket_server.h"
#include "edge_server/websocket/websocket_session.h"
#include <libwebsockets.h>
#include <iostream>
#include <cstring>
#include <algorithm>

namespace edge_server
{
static WebSocketServer* gs_instance = nullptr;

static bool sendToWSClientItance(struct lws* wsi, const std::string& msg)
{
    std::vector<unsigned char> buffer(LWS_PRE + msg.size());
    memcpy(buffer.data() + LWS_PRE, msg.data(), msg.size());

    int ret = lws_write(wsi, buffer.data() + LWS_PRE, msg.size(), LWS_WRITE_TEXT);
    return ret >= 0;
}

static int wsCallback(struct lws* wsi,  enum lws_callback_reasons reason, void* /*user*/, void* in,  size_t len)
{
    switch(reason)
    {
    case LWS_CALLBACK_ESTABLISHED:
    {
        gs_instance->addClientSession(wsi);
        std::cout << "client connected" << std::endl;
        break;
    }
    break;
    case LWS_CALLBACK_RECEIVE:
    {
        std::string msg(static_cast<char*>(in), len);
        gs_instance->pushReceiveMessage(wsi, msg);
        break;
    }
    case LWS_CALLBACK_CLOSED:
    {
        gs_instance->removeClientSession(wsi);
        std::cout << "client disconnect" << std::endl;
        break;
    }
    case LWS_CALLBACK_SERVER_WRITEABLE:
    {
        while(true)
        {
            auto msg = gs_instance->popQueueMsg(wsi);
            if(msg.empty())
                break;

            sendToWSClientItance(wsi, msg);
        }
        
        break;
    }
    default:
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
        if(context)
        {
            lws_context_destroy(context);
            context = nullptr;
        }
    }
};

WebSocketServer::WebSocketServer()
{
    gs_instance = this;
    m_impl = std::make_unique<Impl>();
}

WebSocketServer::~WebSocketServer()
{
    stop();

    if(gs_instance == this)
        gs_instance = nullptr;
}

bool WebSocketServer::start(const std::string &/*host*/, int port)
{
    if(m_running)
        return false;
    
    m_impl->protocols[0] =
     {
        "edge-protocol",
        wsCallback,
        sizeof(void*),
        4096,
        0,
        nullptr,
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
    if(m_thread.joinable())
        m_thread.join();
    
    m_receiveCv.notify_all();
    
    if(m_messageThread.joinable())
        m_messageThread.join();
}

void WebSocketServer::sendToWSClient(lws * wsi)
{
    gs_instance->triggerWritable(wsi);
}

void WebSocketServer::triggerWritable(struct lws *wsi)
{
    lws_callback_on_writable(wsi);
    lws_cancel_service(m_impl->context);
}

void WebSocketServer::setMessageCallback(MessageCallback callback)
{
    m_message_callback = callback;
}

void WebSocketServer::serviceThread()
{
    while(m_running)
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
            m_message_callback(getClientSessionId(message.client), message.data);
        }
    }
}

void WebSocketServer::addClientSession(struct lws* wsi)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(m_clientSession.find(wsi) != m_clientSession.end())
        m_clientSession.erase(wsi);
    
    auto session = std::make_shared<WebSocketSession>(wsi);
    m_clientSession.insert(std::make_pair(wsi, session));
}

void WebSocketServer::removeClientSession(struct lws* wsi)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(m_clientSession.find(wsi) != m_clientSession.end())
        m_clientSession.erase(wsi);
}

uint64_t WebSocketServer::getClientSessionId(struct lws* wsi)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(m_clientSession.find(wsi) != m_clientSession.end())
    {
        std::shared_ptr<WebSocketSession> session = m_clientSession.at(wsi);
        return session->getClientSessionId();
    }

    return 0;
}

void WebSocketServer::pushReceiveMessage(struct lws* wsi, const std::string &message)
{
    std::unique_lock<std::mutex> lock(m_receiveMutex);
    WebSocketMessage msg;
    msg.client = wsi;
    msg.data = message;
    m_receiveQueue.push(msg);
    m_receiveCv.notify_one();
}

std::string WebSocketServer::popQueueMsg(struct lws* wsi)
{
    std::string msg;
    std::shared_ptr<WebSocketSession> session;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if(m_clientSession.find(wsi) == m_clientSession.end())
            return msg;

        session = m_clientSession.at(wsi);
    }

    msg = session->popMessage();
    return msg;
}

}