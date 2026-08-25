#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>

struct lws_context;
struct lws;
struct lws_protocols;

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

private:
    void run();
    bool parseUrl(const std::string& url);

public:
    MessageCallback m_callback;
    std::atomic_bool m_connected{false};
    std::queue<std::string> m_sendQueue;
    std::mutex m_sendMutex;

private:
    std::string m_url;
    std::string m_host;
    std::string m_path;
    int m_port{80};
    struct lws_context* m_context{nullptr};
    struct lws* m_wsi{nullptr};
    struct lws_protocols* m_protocols[2];

    std::thread m_thread;
    std::atomic_bool m_running{false};
};

}
