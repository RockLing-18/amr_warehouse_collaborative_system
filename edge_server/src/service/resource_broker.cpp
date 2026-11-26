#include "service/resource_broker.h"
#include "database/sqlite_db.h"

namespace edge_server
{

bool ResourceBroker::tryGrant(const ResourceLease& lease, bool& granted, int& queue_seq)
{
    auto& db = SQLiteDB::Instance();
    return db.TryGrantLease(lease, granted, queue_seq);
}

bool ResourceBroker::cancelOrRelease(
    const std::string& request_id,
    const std::string& resource_type,
    const std::string& resource_id,
    std::string& promoted_holder,
    std::string& promoted_request_id)
{
    auto& db = SQLiteDB::Instance();
    return db.CancelOrReleaseLease(
        request_id, resource_type, resource_id, promoted_holder, promoted_request_id);
}

bool ResourceBroker::expireLeases(const std::string& now, std::vector<ResourceLease>& expired)
{
    auto& db = SQLiteDB::Instance();
    return db.ExpireLeases(now, expired);
}

}
