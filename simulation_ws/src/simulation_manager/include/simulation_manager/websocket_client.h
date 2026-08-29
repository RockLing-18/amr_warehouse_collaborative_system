#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <queue>
#include <condition_variable>

namespace simulation_manager
{

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
    bool connect(const std::string& url);
    void close();
    bool send(const std::string& message);
    void setEventCallback(EventCallback callback);
    bool isConnected() const;
    void pushQueueMsg(const std::string& message);
    std::string popQueueMsg();
    bool isEmptyQueueMsg();

    // 接收消息入队列
    void pushEventMessage(const WebSocketEvent& eventMsg);

private:
    void run();
    bool parseUrl(const std::string& url);
    void messageThread();

public:
    std::atomic_bool m_connected{false};

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    EventCallback m_callback;
    std::string m_url;
    std::string m_host;
    std::string m_path;
    int m_port{80};
    std::thread m_thread;
    std::atomic_bool m_running{false};
    std::queue<std::string> m_sendQueue;
    std::mutex m_sendMutex;

    std::queue<WebSocketEvent> m_receiveQueue;
    std::mutex m_receiveMutex;
    std::condition_variable m_receiveCv;
    std::thread m_messageThread;
};

}
