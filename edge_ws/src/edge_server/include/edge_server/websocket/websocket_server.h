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
#include "edge_server/common/identifierGenerator.h"

struct lws;

namespace edge_server
{
struct WebSocketMessage
{
    uint64_t client{0};
    std::string data;
};

class WebSocketServer
{
public:
    using MessageCallback = std::function<void(uint64_t, const std::string&)>;

public:
    WebSocketServer();
    ~WebSocketServer();

    bool start(const std::string& host, int port);
    void stop();
    void setMessageCallback(MessageCallback callback);

    uint64_t addClientSession(struct lws* wsi);
    void removeClientSession(uint64_t clientSessionId);

    // 业务层调用
    bool sendToWSClient(uint64_t clientSessionId, const std::string& message);
    
    // 接收消息入队列
    void pushReceiveMessage(uint64_t clientSessionId, const std::string& message);
    // 发送消息入/出队列,从WebSocketSession的消息队列操作
    std::shared_ptr<WebSocketSession> pushSendMsg(uint64_t clientSessionId, const std::string& message);
    std::string popSendMsg(uint64_t clientSessionId);
    bool hasMoreSendMsg(uint64_t clientSessionId);

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
    std::unordered_map<uint64_t, std::shared_ptr<WebSocketSession>> m_sessionById;
    std::mutex m_sessionMutex;

    std::queue<WebSocketMessage> m_receiveQueue;
    std::mutex  m_receiveMutex;
    std::condition_variable m_receiveCv;
    std::thread m_messageThread;
    IdGeneratorU64_t m_idGenerator;
};


}