#pragma once

#include <string>
#include <queue>
#include <mutex>

struct lws;

namespace edge_server
{

class WebSocketSession
{
public:
    explicit WebSocketSession(struct lws* wsi);

    struct lws* getWsi() const;


    // 业务层调用
    void sendToClient(const std::string& message);

    // websocket线程调用
    std::string popMessage();

private:
    struct lws* m_wsi{nullptr};
    std::mutex m_mutex;
    std::queue<std::string> m_sendQueue;
};

}