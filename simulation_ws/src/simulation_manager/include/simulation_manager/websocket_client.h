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
    using MessageCallback = std::function<void(const std::string&)>;
    

public:
    WebSocketClient();
    ~WebSocketClient();
    bool connect(const std::string& url);
    void close();
    bool send(const std::string& message);
    void setMessageCallback(MessageCallback callback);
    bool isConnected() const;
    void pushQueueMsg(const std::string& message);
    std::string popQueueMsg();
    bool isEmptyQueueMsg();

private:
    void run();
    bool parseUrl(const std::string& url);

public:
    MessageCallback m_callback;
    std::atomic_bool m_connected{false};
    std::queue<std::string> m_sendQueue;
    std::mutex m_sendMutex;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    std::string m_url;
    std::string m_host;
    std::string m_path;
    int m_port{80};
    std::thread m_thread;
    std::atomic_bool m_running{false};
};

}
