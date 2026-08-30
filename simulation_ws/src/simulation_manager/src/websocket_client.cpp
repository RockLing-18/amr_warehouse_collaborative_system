#include "simulation_manager/websocket_client.h"
#include <libwebsockets.h>
#include <iostream>
#include <cstring>
#include <vector>

namespace simulation_manager
{

static bool sendToWS(struct lws* wsi, const std::string& msg)
{
    if(!wsi)
        return false;

    std::vector<unsigned char> buffer(LWS_PRE + msg.size());
    memcpy(buffer.data() + LWS_PRE, msg.data(), msg.size());

    int ret = lws_write(wsi, buffer.data() + LWS_PRE, msg.size(), LWS_WRITE_TEXT);
    return ret >= 0;
}

static bool sendPingToWs(struct lws* wsi)
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

static int callback(struct lws* wsi, enum lws_callback_reasons reason, void* /*user*/, void* in, size_t len)
{
    if(!wsi)
        return 0;

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
        client->onConnected();
        break;
    }
    case LWS_CALLBACK_CLIENT_RECEIVE:
    {
        std::string msg(static_cast<char*>(in), len);
        client->onReceive(msg);
        break;
    }
    case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
    {
        client->onUpdateHeartStatus();
        break;
    }
    case LWS_CALLBACK_CLIENT_CLOSED:
    {
        client->onConnectionClosed();
        break;
    }
    case LWS_CALLBACK_CLIENT_WRITEABLE:
    {
        client->onWrite();
        break;
    }
    case LWS_CALLBACK_EVENT_WAIT_CANCELLED:
    {
        // if(client->isConnected())
        // {
        //     lws_callback_on_writable(wsi);
        // }

        break;
    }
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
    {
        client->onConnectionError();
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
    std::atomic<lws_context*> context{nullptr};
    std::atomic<lws*> wsi{nullptr};

    Impl()
    {
        memset(protocols, 0, sizeof(protocols));
    }

    ~Impl()
    {
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

bool WebSocketClient::connect(const std::string& url, const std::string& protocolName, const WebSocketClientOptions& options)
{
    if(m_running.load())
    {
        std::cerr << "[WebSocketClient] already running, skip connect" << std::endl;
        return false;
    }

    if(!parseUrl(url))
        return false;

    m_protocolName = protocolName;
    m_url = url;
    m_options = options;

    if(m_options.reconnectInitialDelay.count() <= 0)
        m_options.reconnectInitialDelay = std::chrono::seconds(1);

    if(m_options.reconnectMaxDelay < m_options.reconnectInitialDelay)
        m_options.reconnectMaxDelay = m_options.reconnectInitialDelay;

    m_currentReconnectDelay = std::chrono::duration_cast<std::chrono::milliseconds>(m_options.reconnectInitialDelay);

    m_connected = false;
    m_running = true;

    m_messageThread = std::thread(&WebSocketClient::messageThread, this);
    m_thread = std::thread(&WebSocketClient::run, this);
    return true;
}

void WebSocketClient::close()
{
    bool wasRunning = m_running.exchange(false);
    m_connected = false;
    auto* context = m_impl->context.load();
    if(context)
        lws_cancel_service(context);
    
    m_receiveCv.notify_all();

    if(m_thread.joinable())
        m_thread.join();

    if(m_messageThread.joinable())
        m_messageThread.join();

    // run线程已经退出，此时再安全销毁context
    context = m_impl->context.exchange(nullptr);
    if(context)
    {
        lws_context_destroy(context);
    }
    m_impl->wsi.store(nullptr);
    
    if(wasRunning)
    {
        std::cout << "[WebSocketClient] closed" << std::endl;
    }
}

void WebSocketClient::setEventCallback(EventCallback callback)
{
    m_callback = std::move(callback);
}

bool WebSocketClient::isConnected() const
{
    return m_connected.load();
}

void WebSocketClient::pushQueueMsg(const std::string& message)
{
    std::lock_guard<std::mutex> lock(m_sendMutex);
    m_sendQueue.push(message);
}

std::string WebSocketClient::popQueueMsg()
{
    std::lock_guard<std::mutex> lock(m_sendMutex);
    if(m_sendQueue.empty())
        return "";

    auto message = std::move(m_sendQueue.front());
    m_sendQueue.pop();
    return message;
}

bool WebSocketClient::isEmptyQueueMsg()
{
    std::lock_guard<std::mutex> lock(m_sendMutex);
    return m_sendQueue.empty();
}

void WebSocketClient::run()
{
    m_impl->protocols[0] =
    {
        m_protocolName.c_str(),
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

    auto* context = lws_create_context(&info);
    if(!context)
     {
        std::cerr <<"create lws context failed"  <<std::endl;
        m_running = false;
        m_receiveCv.notify_all();
        return;
    }

    m_impl->context.store(context);

    m_nextReconnectTime = std::chrono::steady_clock::now();

    while(m_running)
    {
        // 当前没有连接
        if(!m_connected)
        {
            auto now = std::chrono::steady_clock::now();
            if(now >= m_nextReconnectTime)
            {
                // 如果当前没有wsi，创建一个新的连接
                if(!m_impl->wsi.load())
                {
                    std::cout
                        << "[WebSocketClient] "
                        << "try connect "
                        << m_host
                        << ":"
                        << m_port
                        << m_path
                        << std::endl;

                    // 注意：context已经存在，这里只创建连接
                    lws_client_connect_info ccinfo{};
                    ccinfo.context = context;
                    ccinfo.address = m_host.c_str();
                    ccinfo.port = m_port;
                    ccinfo.path = m_path.c_str();
                    ccinfo.host = m_host.c_str();
                    ccinfo.origin = m_host.c_str();
                    ccinfo.protocol = m_impl->protocols[0].name;
                    auto* wsi = lws_client_connect_via_info(&ccinfo);
                    if(wsi)
                        m_impl->wsi.store(wsi);
                    else
                        scheduleReconnect();
                }
            }
        }

        // LWS事件循环
        lws_service(context, m_options.serviceTimeoutMs);

        // 心跳
        checkHeartbeat();
    }

    // m_connected = false;
    // auto* wsi = m_impl->wsi.exchange(nullptr);
    // if(wsi)
    // {
    //     /*
    //      * LWS context销毁时，
    //      * 会清理连接。
    //      */
    // }
    // m_impl->context.store(nullptr);
    // lws_context_destroy(context);
}

bool WebSocketClient::send(const std::string& message)
{
    pushQueueMsg(message);
    auto* context = m_impl->context.load();

    if(context)
    {
        lws_cancel_service(context);
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
        m_host = hostport;
        m_port = 80;
    }

    return true;
}

void WebSocketClient::messageThread()
{
    while(m_running)
    {
        WebSocketEvent eventMsg;

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

            eventMsg = std::move(m_receiveQueue.front());
            m_receiveQueue.pop();
        }

        if(m_callback)
        {
            m_callback(eventMsg);
        }
    }
}


 void WebSocketClient::pushEventMessage(const WebSocketEvent& eventMsg)
 {
    {
        std::unique_lock<std::mutex> lock(m_receiveMutex);
        m_receiveQueue.push(eventMsg);
    }

    m_receiveCv.notify_one();
 }

 void WebSocketClient::onUpdateHeartStatus()
 {
    m_lastPongTime = std::chrono::steady_clock::now();
    m_pingOutstanding = false;
 }

void WebSocketClient::disconnect()
{
    auto* wsi = m_impl->wsi.load();
    if(!wsi)
    {
        return;
    }

    lws_set_timeout(wsi, PENDING_TIMEOUT_CLOSE_SEND,1);
}

bool WebSocketClient::shouldReconnect() const
{
    return m_running && !m_connected;
}

void WebSocketClient::scheduleReconnect()
{
    if(!m_running)
    {
        return;
    }

    auto now = std::chrono::steady_clock::now();

    m_nextReconnectTime =  now + m_currentReconnectDelay;

    std::cout
        << "[WebSocketClient] "
        << "next reconnect after "
        << m_currentReconnectDelay.count()
        << " ms"
        << std::endl;

    /*
     * 指数退避：
     *
     * 1s
     * 2s
     * 4s
     * 8s
     * 16s
     * 30s
     * 30s
     * ...
     */

    auto maxDelay = std::chrono::duration_cast<std::chrono::milliseconds>(m_options.reconnectMaxDelay);
    auto nextDelay = m_currentReconnectDelay * 2;
    m_currentReconnectDelay = std::min(nextDelay, maxDelay);
}

void WebSocketClient::onConnected()
{
    m_connected = true;
    m_currentReconnectDelay = std::chrono::duration_cast<std::chrono::milliseconds>(m_options.reconnectInitialDelay);
    m_lastPingTime = std::chrono::steady_clock::now();

    WebSocketEvent eventMsg
    {
        EventType::CONNECTED,
        "",
        0
    };

    pushEventMessage(eventMsg);
    std::cout << "[WebSocketClient] connected: "  << m_url << std::endl;
}

void WebSocketClient::checkHeartbeat()
{
    if(!m_connected)
        return;
    
    auto now = std::chrono::steady_clock::now();

    /*
     * 还没有Ping，
     * 或者上一次Ping已经得到Pong。
     *
     * 到时间之后申请writeable，
     * 真正的Ping在callback里发送。
     */
    if(!m_pingOutstanding && now - m_lastPingTime >= m_options.heartbeatInterval)
    {
        auto* wsi = m_impl->wsi.load();
        if(wsi)
            lws_callback_on_writable(wsi);
        
    }

    /*
     * Ping已经发出，但是超过timeout还没有Pong。
     *
     * 认为连接已经失效。
     */

    if(m_pingOutstanding &&  now - m_lastPingTime >= m_options.heartbeatTimeout)
    {
        std::cerr  << "[WebSocketClient] " << "heartbeat timeout"  << std::endl;
        m_pingOutstanding = false;

        /*
         * 主动关闭当前wsi。
         *
         * CLOSED回调随后负责：
         *
         * m_connected = false
         * DISCONNECTED
         * scheduleReconnect
         */

        auto* wsi = m_impl->wsi.load();
        if(wsi)
            lws_set_timeout(wsi, PENDING_TIMEOUT_CLOSE_SEND, 1);
    }
}

void WebSocketClient::onConnectionClosed()
{
    m_impl->wsi.store(nullptr);
    m_pingOutstanding = false;
    scheduleReconnect();

    bool wasConnected = m_connected.exchange(false);
    if(wasConnected)
    {
        WebSocketClient::WebSocketEvent eventMsg
        {
            WebSocketClient::EventType::DISCONNECTED,
            "",
            0
        };

        pushEventMessage(eventMsg);
    }

    std::cout << "[WebSocketClient] disconnected"  << std::endl;
}

void WebSocketClient::onReceive(const std::string& message)
{
    WebSocketClient::WebSocketEvent eventMsg
    {
        WebSocketClient::EventType::MESSAGE,
        message,
        0
    };
    
    pushEventMessage(eventMsg);
}

void WebSocketClient::onWrite()
{
    auto* wsi = m_impl->wsi.load();
    if(!wsi || !m_connected)
        return;

    // 1. Ping优先
    auto now = std::chrono::steady_clock::now();
    if(m_connected && !m_pingOutstanding &&  now - m_lastPingTime >= m_options.heartbeatInterval)
    {
        if(sendPingToWs(wsi))
        {
            m_lastPingTime = now;
            m_pingOutstanding = true;
        }
        else
        {
            /*
            * Ping发送失败。
            * 不直接操作连接，
            * 等heartbeat timeout处理。
            */
        }

        // Ping优先级高于业务消息。
        if(!isEmptyQueueMsg())
            lws_callback_on_writable(wsi);
        
        return;
    }

    // 2. 处理业务发送队列
    auto msg = popQueueMsg();
    if(!msg.empty())
    {
        if(!sendToWS(wsi, msg))
        {
            std::cerr << "[WebSocketClient] " << "send websocket message failed" << std::endl;
        }
    }

    // 3. 队列还有消息，继续申请writeable
    if(!isEmptyQueueMsg())
        lws_callback_on_writable(wsi);
}

void WebSocketClient::onConnectionError()
{
    m_impl->wsi.store(nullptr);
    m_connected = false;
    WebSocketClient::WebSocketEvent eventMsg
    {
        WebSocketClient::EventType::ERROR,
        "websocket connection error",
        -1
    };

    pushEventMessage(eventMsg);

    m_pingOutstanding = false;
    scheduleReconnect();

    std::cerr << "[WebSocketClient] connection error" << std::endl;
}

}
