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
#include "identifierGenerator.h"

struct lws;

namespace edge_server
{
struct WebSocketMessage
{
    uint64_t client{0};
    std::string data;
};

struct WebSocketServerOptions 
{
    // Ping发送间隔 
    std::chrono::seconds heartbeatInterval{30}; 
    // Ping发送后，等待Pong的最大时间 
    std::chrono::seconds heartbeatTimeout{10}; 
    // LWS service timeout 
    int serviceTimeoutMs{100}; 
};

class WebSocketSession;

class WebSocketServer
{
public:
    using MessageCallback = std::function<void(uint64_t, const std::string&)>;

public:
    WebSocketServer();
    ~WebSocketServer();

    bool start(const std::string& host, int port, const std::string& protocolName, const WebSocketServerOptions& options = {});
    void stop();
    void setMessageCallback(MessageCallback callback);

    uint64_t onConnected(struct lws *wsi);
    void onDisconnected(uint64_t clientSessionId);
    void onWriteable(uint64_t clientSessionId);
    void onReceive(uint64_t clientSessionId, const std::string& message);
    void onPong(uint64_t clientSessionId);

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

    // 心跳 
    void checkHeartbeat(); 
    void triggerHeartbeat(struct lws* wsi); 
    // Session
    std::shared_ptr<WebSocketSession> addClientSession(struct lws* wsi);
    std::shared_ptr<WebSocketSession> removeClientSession(uint64_t clientSessionId);
    std::shared_ptr<WebSocketSession> getClientSession(uint64_t clientSessionId); 
    // 删除并关闭Session 
    void cleanupSession(uint64_t clientSessionId);

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
    WebSocketServerOptions m_options;
};


}