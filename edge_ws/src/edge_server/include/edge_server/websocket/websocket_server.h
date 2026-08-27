#pragma once

#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>
#include <memory>
#include <queue>
#include <unordered_map>
#include <condition_variable>
#include "edge_server/websocket/websocket_session.h"

struct lws;

namespace edge_server
{
struct WebSocketMessage
{
    struct lws* client{nullptr};
    std::string data;
};

class WebSocketServer
{
public:
    using MessageCallback = std::function<void(lws* client, const std::string&)>;

public:
    WebSocketServer();
    ~WebSocketServer();

    bool start(const std::string& host, int port);
    void stop();

    static void sendToWSClient(struct lws* wsi);

    void setMessageCallback(MessageCallback callback);
    void addClientSession(struct lws* wsi);
    void removeClientSession(struct lws* wsi);
    void pushReceiveMessage(struct lws* wsi, const std::string& message);
    std::string popQueueMsg(struct lws* wsi);

private:
    void serviceThread();
    void messageThread();
    void triggerWritable(struct lws *wsi);

private:
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    MessageCallback m_message_callback;
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::unordered_map<lws*, std::shared_ptr<WebSocketSession>> m_clientSession;
    std::mutex m_mutex;

    std::queue<WebSocketMessage> m_receiveQueue;
    std::mutex  m_receiveMutex;
    std::condition_variable m_receiveCv;
    std::thread m_messageThread;
};


}