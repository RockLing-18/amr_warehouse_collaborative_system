#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

namespace amr_agent
{

struct PendingReport
{
    std::string message_id;
    std::string topic;
    std::string payload;
};

class PendingReportStore
{
public:
    explicit PendingReportStore(const std::string& file_path);

    void add(const PendingReport& report);
    void removeByMessageIds(const std::vector<std::string>& message_ids);
    std::vector<PendingReport> load() const;

private:
    void save(const std::vector<PendingReport>& reports) const;
    nlohmann::json toJson(const std::vector<PendingReport>& reports) const;

private:
    std::string m_file_path;
    mutable std::mutex m_mutex;
};

}
