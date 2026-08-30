#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <queue>
#include <chrono>
#include <condition_variable>

namespace simulation_manager
{

struct WebSocketClientOptions
{
    // 心跳
    std::chrono::seconds heartbeatInterval{30};
    std::chrono::seconds heartbeatTimeout{10};

    // 重连
    std::chrono::seconds reconnectInitialDelay{1};
    std::chrono::seconds reconnectMaxDelay{30};

    // LWS service timeout
    int serviceTimeoutMs{100};
};

class WebSocketClient
{
public:
    enum class EventType
    {
        CONNECTED,          // websocket建立成功
        DISCONNECTED,       // websocket断开
        MESSAGE,            // 收到消息
        ERROR               // 连接异常
    };

    struct WebSocketEvent
    {
        EventType type;
        std::string message;
        int errorCode{0};
    };

    using EventCallback = std::function<void(const WebSocketEvent&)>;
    
public:
    WebSocketClient();
    ~WebSocketClient();
    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;

    bool connect(const std::string& url, const std::string& protocolName, const WebSocketClientOptions& options = WebSocketClientOptions{});
    void close();
    bool send(const std::string& message);
    void setEventCallback(EventCallback callback);
    bool isConnected() const;

    void onConnected();
    void onConnectionClosed();
    void onReceive(const std::string& message);
    void onWrite();
    void onUpdateHeartStatus();
    void onConnectionError();

private:
    void run();

    // 关闭当前连接
    void disconnect();

    // 心跳检测
    void checkHeartbeat();

    // 判断是否需要重连
    bool shouldReconnect() const;

    // 计算下一次重连时间
    void scheduleReconnect();

    bool parseUrl(const std::string& url);

    void messageThread();
    // 接收消息入队列
    void pushEventMessage(const WebSocketEvent& eventMsg);

    // 写消息队列操作
    void pushQueueMsg(const std::string& message);
    std::string popQueueMsg();
    bool isEmptyQueueMsg();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    EventCallback m_callback;
    std::string m_url;
    std::string m_protocolName;
    std::string m_host;
    std::string m_path;
    int m_port{80};
    WebSocketClientOptions m_options;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_connected{false};
    std::queue<std::string> m_sendQueue;
    std::mutex m_sendMutex;
     
    // 重连
    std::chrono::milliseconds m_currentReconnectDelay{1000};
    std::chrono::steady_clock::time_point  m_nextReconnectTime;

    // 心跳
    std::chrono::steady_clock::time_point m_lastPingTime{};
    std::chrono::steady_clock::time_point m_lastPongTime{};
    std::atomic<bool> m_pingOutstanding{false};

    std::queue<WebSocketEvent> m_receiveQueue;
    std::mutex m_receiveMutex;
    std::condition_variable m_receiveCv;
    std::thread m_messageThread;
};

}
