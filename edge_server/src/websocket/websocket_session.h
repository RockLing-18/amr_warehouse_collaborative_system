#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <atomic>
#include <chrono>

struct lws;

namespace edge_server
{

class WebSocketSession
{
public: 
    using Clock = std::chrono::steady_clock;

public:
    explicit WebSocketSession(struct lws* wsi, uint64_t id);

    uint64_t getClientId() const;
    lws* getWsi() const;

    void setClientIp(const std::string& ip);
    std::string getClientIp() const;

    // 业务发送队列
    bool pushMessage(const std::string& message);
    std::string popMessage();
    bool isEmptyMessage();

    // WebSocket 生命周期
    void setWsiInvalid();
    bool isWsiValid() const;

    // 心跳 
    void updatePong(); 
    void markPingSent(); 
    bool isPingOutstanding() const; 
    Clock::time_point getLastPongTime() const; 
    Clock::time_point getLastPingTime() const;
private:
    std::atomic<struct lws*> m_wsi{nullptr};
    uint64_t m_clientSessionId{0};
    std::string m_clientIp;
    std::mutex m_mutex;
    std::queue<std::string> m_sendQueue;

    // 心跳状态 
    std::atomic<Clock::time_point> m_lastPongTime; 
    std::atomic<Clock::time_point> m_lastPingTime; 
    std::atomic<bool> m_pingOutstanding{false};

    static constexpr size_t MAX_SEND_QUEUE = 2000;
};

}