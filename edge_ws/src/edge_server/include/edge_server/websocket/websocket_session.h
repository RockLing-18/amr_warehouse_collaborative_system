#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <atomic>

struct lws;

namespace edge_server
{

class WebSocketSession
{
public:
    explicit WebSocketSession(struct lws* wsi, uint64_t id);

    lws* getWsi() const;

    void pushMessage(const std::string& message);
    std::string popMessage();
    bool isEmptyMessage();
    void setWsiInvalid();
    bool isWsiValid() const;
private:
    std::atomic<struct lws*> m_wsi{nullptr};
    uint64_t m_clientSessionId;
    std::mutex m_mutex;
    std::queue<std::string> m_sendQueue;
};

}