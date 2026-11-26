#pragma once

#include <functional>
#include <string>
#include <vector>
#include "types.h"

namespace edge_server
{

class ResourceBroker
{
public:
    bool tryGrant(const ResourceLease& lease, bool& granted, int& queue_seq);
    bool cancelOrRelease(
        const std::string& request_id,
        const std::string& resource_type,
        const std::string& resource_id,
        std::string& promoted_holder,
        std::string& promoted_request_id);
    bool expireLeases(const std::string& now, std::vector<ResourceLease>& expired);
};

}
