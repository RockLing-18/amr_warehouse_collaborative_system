#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include "types.h"

namespace edge_server
{

class TrafficRightsService
{
public:
    using PublishCallback = std::function<bool(const std::string& topic, const std::string& payload)>;

    void init();
    void setPublishCallback(PublishCallback callback);
    bool handleRequest(const TrafficRightsRequest& request, TrafficRightsResponse& response);

private:
    void publishResponse(const TrafficRightsResponse& response);
    void expiryLoop();
    std::string nowIso8601() const;
    std::string isoAfterSeconds(double seconds) const;

private:
    PublishCallback m_publish_callback;
    std::atomic<bool> m_running{false};
    std::thread m_expiry_thread;
};

}
