#include "simulation_manager/websocket_client.h"
#include <libwebsockets.h>
#include <iostream>
#include <cstring>
#include <vector>

namespace simulation_manager
{

static bool sendToWS(struct lws* wsi, const std::string& msg)
{
    std::vector<unsigned char> buffer(LWS_PRE + msg.size());
    memcpy(buffer.data()+LWS_PRE, msg.data(), msg.size());

    int ret = lws_write(wsi, buffer.data()+LWS_PRE, msg.size(), LWS_WRITE_TEXT);
    return ret >= 0;
}

static int callback(struct lws* wsi, enum lws_callback_reasons reason, void* /*user*/, void* in, size_t len)
{
    auto protocol = lws_get_protocol(wsi);
    if(!protocol)
        return 0;
        
    auto client = static_cast<WebSocketClient*>(protocol->user);
    if(!client)
        return 0;
    
    switch(reason)
    {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
    {
        client->m_connected = true;
        std::cout << "connect succeed! " << std::endl;
        break;
    }
    case LWS_CALLBACK_CLIENT_RECEIVE:
    {
        std::string msg(static_cast<char*>(in), len);
        if(client->m_callback)
            client->m_callback(msg);
    
        break;
    }
    case LWS_CALLBACK_CLIENT_CLOSED:
    {
        client->m_connected = false;
        std::cout << "websocket closed" << std::endl;
        break;
    }
    case LWS_CALLBACK_CLIENT_WRITEABLE:
    {
        while(true)
        {
            auto msg = client->popQueueMsg();
            if(msg.empty())
                break;

            sendToWS(wsi, msg);
        }
        
        break;
    }
    case LWS_CALLBACK_EVENT_WAIT_CANCELLED:
    {
        lws_callback_on_writable(wsi);
        break;
    }
    default:
        break;
    }

    return 0;
}

struct WebSocketClient::Impl
{
    struct lws_protocols protocols[2];
    lws_context* context{nullptr};
    lws* wsi{nullptr};

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

        wsi = nullptr;
    }
};

WebSocketClient::WebSocketClient()
{
    m_impl = std::make_unique<Impl>();

}

WebSocketClient::~WebSocketClient()
{
    close();
}

bool WebSocketClient::connect(const std::string& url)
{
    m_url = url;
    if(!parseUrl(url))
        return false;

    m_running = true;
    m_thread = std::thread(&WebSocketClient::run, this);
    return true;
}

void WebSocketClient::close()
{
    m_running = false;

    if(m_impl->context)
        lws_cancel_service(m_impl->context);
    
    if(m_thread.joinable())
        m_thread.join();

    if(m_impl->context)
    {
        lws_context_destroy(m_impl->context);
        m_impl->context = nullptr;
    }

    m_connected = false;
}


void WebSocketClient::setMessageCallback(MessageCallback callback)
{
    m_callback = callback;
}

bool WebSocketClient::isConnected() const
{
    return m_connected;
}

std::string WebSocketClient::popQueueMsg()
{
    std::string msg;
    {
        std::lock_guard<std::mutex> lock(m_sendMutex);
        if(m_sendQueue.empty())
            return msg;

        msg = m_sendQueue.front();
        m_sendQueue.pop();
    }

    return msg;
}

void WebSocketClient::run()
{
    m_impl->protocols[0] =
    {
        "robot-protocol",
        callback,
        sizeof(void*),
        4096,
        0,
        this,
        0
    };

    lws_context_creation_info info{};
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = m_impl->protocols;

    m_impl->context = lws_create_context(&info);
    if(!m_impl->context)
     {
        std::cerr <<"create lws context failed"  <<std::endl;
        return;
    }

    lws_client_connect_info ccinfo{};
    ccinfo.context = m_impl->context;
    ccinfo.address = m_host.c_str();
    ccinfo.port = m_port;
    ccinfo.path = m_path.c_str();
    ccinfo.host = m_host.c_str();
    ccinfo.origin = m_host.c_str();
    ccinfo.protocol = m_impl->protocols[0].name;

    m_impl->wsi = lws_client_connect_via_info(&ccinfo);
    if(!m_impl->wsi)
    {
         std::cerr <<"connect websocket failed" <<std::endl;
        return;
    }

    while(m_running)
    {
        lws_service(m_impl->context, 100);
    }

}

bool WebSocketClient::send(const std::string& message)
{
    {
        std::lock_guard<std::mutex> lock(m_sendMutex);
        m_sendQueue.push(message);
    }

    if(m_impl->context)
    {
        lws_cancel_service(m_impl->context);
    }

    return true;
}

bool WebSocketClient::parseUrl(const std::string& url)
{
    // 解析处理:
    // ws://host:port/path
    auto pos = url.find("://");
    if(pos == std::string::npos)
        return false;

    auto start = pos + 3;
    auto slash = url.find("/", start);
    std::string hostport;
    if(slash == std::string::npos)
    {
        hostport = url.substr(start);
        m_path = "/";
    }
    else
    {
        hostport = url.substr(start, slash-start);
        m_path = url.substr(slash);
    }

    auto colon = hostport.find(':');
    if(colon != std::string::npos)
    {
        m_host = hostport.substr(0, colon);
        m_port = std::stoi(hostport.substr(colon + 1));
    }
    else
    {
        m_host=hostport;
        m_port=80;
    }

    return true;
}

}
