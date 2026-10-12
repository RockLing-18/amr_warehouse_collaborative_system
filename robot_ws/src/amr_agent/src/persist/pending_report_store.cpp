#include "persist/pending_report_store.h"

#include <filesystem>
#include <fstream>
#include <algorithm>

namespace fs = std::filesystem;

namespace amr_agent
{

PendingReportStore::PendingReportStore(const std::string& file_path)
    : m_file_path(file_path)
{
}

void PendingReportStore::add(const PendingReport& report)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto reports = load();
    auto it = std::find_if(
        reports.begin(),
        reports.end(),
        [&report](const PendingReport& item)
        {
            return item.message_id == report.message_id;
        });

    if(it != reports.end())
        return;

    reports.push_back(report);
    save(reports);
}

void PendingReportStore::removeByMessageIds(const std::vector<std::string>& message_ids)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto reports = load();
    reports.erase(
        std::remove_if(
            reports.begin(),
            reports.end(),
            [&message_ids](const PendingReport& item)
            {
                return std::find(message_ids.begin(), message_ids.end(), item.message_id) != message_ids.end();
            }),
        reports.end());
    save(reports);
}

std::vector<PendingReport> PendingReportStore::load() const
{
    std::vector<PendingReport> reports;
    std::ifstream ifs(m_file_path);
    if(!ifs.is_open())
        return reports;

    nlohmann::json root;
    try
    {
        ifs >> root;
        for(const auto& item : root.value("reports", nlohmann::json::array()))
        {
            PendingReport report;
            report.message_id = item.value("message_id", "");
            report.topic = item.value("topic", "");
            report.payload = item.value("payload", "");
            if(!report.message_id.empty() && !report.topic.empty())
                reports.push_back(report);
        }
    }
    catch(...)
    {
        reports.clear();
    }

    return reports;
}

void PendingReportStore::save(const std::vector<PendingReport>& reports) const
{
    fs::path path(m_file_path);
    if(path.has_parent_path())
        fs::create_directories(path.parent_path());

    fs::path tmp_path = path.string() + ".tmp";
    std::ofstream ofs(tmp_path, std::ios::trunc);
    if(!ofs.is_open())
        return;

    ofs << toJson(reports).dump(2);
    ofs.flush();
    ofs.close();
    fs::rename(tmp_path, path);
}

nlohmann::json PendingReportStore::toJson(const std::vector<PendingReport>& reports) const
{
    nlohmann::json root;
    root["version"] = 1;
    root["reports"] = nlohmann::json::array();
    for(const auto& report : reports)
    {
        nlohmann::json item;
        item["message_id"] = report.message_id;
        item["topic"] = report.topic;
        item["payload"] = report.payload;
        root["reports"].push_back(item);
    }
    return root;
}

}
