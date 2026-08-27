#pragma once

#include <string>
#include <queue>
#include <mutex>
#include "edge_server/common/identifierGenerator.h"

struct lws;

namespace edge_server
{

class WebSocketSession
{
public:
    explicit WebSocketSession(struct lws* wsi);

    uint64_t getClientSessionId() const;

    // 业务层调用
    void sendToClient(const std::string& message);

    // websocket线程调用
    std::string popMessage();

private:
    static IdGeneratorU64_t sm_idGenerator;

private:
    struct lws* m_wsi{nullptr};
    uint64_t m_clientSessionId;
    std::mutex m_mutex;
    std::queue<std::string> m_sendQueue;
};

}