#include "simulation_manager/websocket_client.h"
#include <libwebsockets.h>
#include <iostream>
#include <cstring>
#include <vector>

namespace simulation_manager
{

static bool sendToWS(struct lws* wsi, WebSocketClient* client)
{
    std::string msg;
    {
        std::lock_guard<std::mutex> lock(client->m_sendMutex);
        if(client->m_sendQueue.empty())
            return false;

        msg = client->m_sendQueue.front();
        client->m_sendQueue.pop();
    }

    std::vector<unsigned char> buffer(LWS_PRE + msg.size());
    memcpy(buffer.data()+LWS_PRE, msg.data(), msg.size());

    lws_write(wsi, buffer.data()+LWS_PRE, msg.size(), LWS_WRITE_TEXT);
    return true;
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
        client->m_connected = true;
        std::cout << "connect succeed! " << std::endl;
        break;
    case LWS_CALLBACK_CLIENT_RECEIVE:
    {
        std::string msg(static_cast<char*>(in), len);
        if(client->m_callback)
            client->m_callback(msg);
    
        break;
    }
    case LWS_CALLBACK_CLIENT_CLOSED:
        client->m_connected=false;
        std::cout << "websocket closed" << std::endl;
        break;
    case LWS_CALLBACK_CLIENT_WRITEABLE:
        sendToWS(wsi, client);
    break;
    default:
        break;
    }

    return 0;
}

WebSocketClient::WebSocketClient()
{
    m_protocols = new lws_protocols(sizeof(lws_protocols) * 2);
    m_protocols[0]=
    {
        "robot-protocol",
        callback,
        sizeof(void*),
        4096,
        0,
        this,
        0
    };

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
    if(m_thread.joinable())
        m_thread.join();

    if(m_context)
    {
        lws_context_destroy( m_context);
        m_context = nullptr;
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

void WebSocketClient::run()
{
    
    // struct lws_protocols protocols[] =
    // {
    //     {
    //         "robot-protocol",                    // name 
    //         callback,
    //         sizeof(void*),                       // per_session_data_size 
    //         4096,                                // rx_buffer_size 
    //         0,
    //         this,                             // user
    //         0
    //     },
    //     {
    //         nullptr,
    //         nullptr,
    //         0,
    //         0,
    //         0,
    //         nullptr,
    //         0
    //     }
    // };

    lws_context_creation_info info{};
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = m_protocols;

    m_context = lws_create_context(&info);
    if(!m_context)
     {
        std::cerr <<"create lws context failed"  <<std::endl;
        return;
    }

    lws_client_connect_info ccinfo{};
    ccinfo.context = m_context;
    ccinfo.address = m_host.c_str();
    ccinfo.port = m_port;
    ccinfo.path = m_path.c_str();
    ccinfo.host = m_host.c_str();
    ccinfo.origin = m_host.c_str();
    ccinfo.protocol = protocols[0].name;

    m_wsi = lws_client_connect_via_info(&ccinfo);
    if(!m_wsi)
    {
         std::cerr <<"connect websocket failed" <<std::endl;
        return;
    }

    while(m_running)
    {
        lws_service(m_context, 100);
    }

}

bool WebSocketClient::send(const std::string& message)
{
    if(!m_connected || !m_wsi)
        return false;

     {
        std::lock_guard<std::mutex> lock(m_sendMutex);
        m_sendQueue.push(message);
    }

    // if(m_wsi)
    //     lws_callback_on_writable(m_wsi);

    if(m_context)
    {
        lws_cancel_service(m_context);
    }

    return true;

    // std::vector<unsigned char> buffer(LWS_PRE + message.size());
    // memcpy(buffer.data() + LWS_PRE, message.data(), message.size());

    // int ret = lws_write(m_wsi, buffer.data() + LWS_PRE, message.size(), LWS_WRITE_TEXT);
    // return ret >= 0;
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
