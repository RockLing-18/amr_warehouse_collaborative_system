#include "service/traffic_rights_service.h"
#include "service/resource_broker.h"
#include "utils/LogDefine.h"
#include "mqtt/mqtt_topic.h"
#include "nlohmann/json.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace edge_server
{
using json = nlohmann::json;

void TrafficRightsService::init()
{
    if(m_running.exchange(true))
        return;
    m_expiry_thread = std::thread(&TrafficRightsService::expiryLoop, this);
}

void TrafficRightsService::setPublishCallback(PublishCallback callback)
{
    m_publish_callback = std::move(callback);
}

bool TrafficRightsService::handleRequest(
    const TrafficRightsRequest& request,
    TrafficRightsResponse& response)
{
    response.request_id = request.request_id;
    response.robot_id = request.robot_id;
    response.resource_id = request.resource_id;
    response.epoch = request.epoch;

    if(request.request_id.empty() || request.robot_id.empty() || request.resource_id.empty())
    {
        response.result = "REJECTED";
        response.reason = "missing required field";
        return false;
    }

    ResourceBroker broker;
    if(request.op == "REQUEST")
    {
        ResourceLease lease;
        lease.request_id = request.request_id;
        lease.resource_type = "TRAFFIC_RIGHT";
        lease.resource_id = request.resource_id;
        lease.holder_robot_id = request.robot_id;
        lease.epoch = request.epoch;
        lease.lease_ttl_sec = request.lease_ttl_sec > 0 ? request.lease_ttl_sec : 60.0;
        lease.granted_at = nowIso8601();
        lease.lease_expires_at = isoAfterSeconds(lease.lease_ttl_sec);

        bool granted = false;
        int queue_seq = -1;
        if(!broker.tryGrant(lease, granted, queue_seq))
        {
            response.result = "REJECTED";
            response.reason = "broker grant failed";
            return false;
        }

        response.result = granted ? "GRANTED" : "PENDING";
        response.queue_seq = queue_seq;
        if(granted)
        {
            double now_epoch = std::chrono::duration<double>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            response.lease_expires_at = now_epoch + lease.lease_ttl_sec;
        }
        return true;
    }

    if(request.op == "CANCEL" || request.op == "RELEASE")
    {
        std::string promoted_holder;
        std::string promoted_request_id;
        if(!broker.cancelOrRelease(
            request.request_id,
            "TRAFFIC_RIGHT",
            request.resource_id,
            promoted_holder,
            promoted_request_id))
        {
            response.result = "REJECTED";
            response.reason = "lease not found";
            return false;
        }

        response.result = request.op == "CANCEL" ? "CANCELLED" : "RELEASED";

        if(!promoted_request_id.empty())
        {
            TrafficRightsResponse promoted;
            promoted.request_id = promoted_request_id;
            promoted.robot_id = promoted_holder;
            promoted.resource_id = request.resource_id;
            promoted.result = "GRANTED";
            promoted.queue_seq = 0;
            publishResponse(promoted);
        }
        return true;
    }

    response.result = "REJECTED";
    response.reason = "unknown op";
    return false;
}

void TrafficRightsService::publishResponse(const TrafficRightsResponse& response)
{
    if(!m_publish_callback)
        return;

    json payload;
    payload["request_id"] = response.request_id;
    payload["robot_id"] = response.robot_id;
    payload["resource_id"] = response.resource_id;
    payload["epoch"] = response.epoch;
    payload["result"] = response.result;
    payload["queue_seq"] = response.queue_seq;
    payload["lease_expires_at"] = response.lease_expires_at;
    payload["reason"] = response.reason;

    std::string topic = mqtt_topic::makeAmrTrafficResponseTopic(response.robot_id);
    m_publish_callback(topic, payload.dump());
}

void TrafficRightsService::expiryLoop()
{
    while(m_running)
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        ResourceBroker broker;
        std::vector<ResourceLease> expired;
        if(!broker.expireLeases(nowIso8601(), expired))
            continue;

        for(const auto& lease : expired)
        {
            TrafficRightsResponse response;
            response.request_id = lease.request_id;
            response.robot_id = lease.holder_robot_id;
            response.resource_id = lease.resource_id;
            response.result = "EXPIRED";
            publishResponse(response);

            std::string promoted_holder;
            std::string promoted_request_id;
            if(broker.cancelOrRelease(
                lease.request_id,
                lease.resource_type,
                lease.resource_id,
                promoted_holder,
                promoted_request_id) && !promoted_request_id.empty())
            {
                TrafficRightsResponse promoted;
                promoted.request_id = promoted_request_id;
                promoted.robot_id = promoted_holder;
                promoted.resource_id = lease.resource_id;
                promoted.result = "GRANTED";
                promoted.queue_seq = 0;
                publishResponse(promoted);
            }
        }
    }
}

std::string TrafficRightsService::nowIso8601() const
{
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&tt, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S%z");
    return oss.str();
}

std::string TrafficRightsService::isoAfterSeconds(double seconds) const
{
    auto now = std::chrono::system_clock::now() +
               std::chrono::milliseconds(static_cast<int64_t>(seconds * 1000.0));
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&tt, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S%z");
    return oss.str();
}

}
