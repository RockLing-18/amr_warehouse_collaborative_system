#include "service/idle_point_allocator.h"

#include <cmath>
#include <zip.h>
#include <yaml-cpp/yaml.h>

#include "utils/LogDefine.h"

namespace edge_server
{

bool IdlePointAllocator::load(const std::string& mapPackagePath)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_points.clear();
    m_occupied.clear();
    m_idleZoneIds.clear();

    std::string zonesContent;
    if(extractFile(mapPackagePath, "zones.yaml", zonesContent))
    {
        parseZones(zonesContent);
    }

    std::string stationsContent;
    if(!extractFile(mapPackagePath, "stations.yaml", stationsContent))
    {
        LOG_ERROR("extract stations.yaml failed path={}", mapPackagePath);
        return false;
    }

    return parseStations(stationsContent);
}

bool IdlePointAllocator::allocate(Pose2D& pose)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for(size_t i = 0; i < m_points.size(); ++i)
    {
        if(!m_occupied[i])
        {
            m_occupied[i] = true;
            pose = m_points[i].pose;

            LOG_INFO(
                "allocate idle point station={} location={} x={} y={} yaw={}",
                m_points[i].station_id,
                m_points[i].location_id,
                pose.x,
                pose.y,
                pose.yaw);
            return true;
        }
    }

    LOG_WARN("no free idle point available");
    return false;
}

void IdlePointAllocator::markOccupied(const Pose2D& pose)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for(size_t i = 0; i < m_points.size(); ++i)
    {
        if(std::fabs(m_points[i].pose.x - pose.x) < 1e-6 &&
           std::fabs(m_points[i].pose.y - pose.y) < 1e-6 &&
           std::fabs(m_points[i].pose.yaw - pose.yaw) < 1e-6)
        {
            m_occupied[i] = true;
            return;
        }
    }
}

bool IdlePointAllocator::empty() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_points.empty();
}

size_t IdlePointAllocator::size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_points.size();
}

bool IdlePointAllocator::extractFile(
    const std::string& zipPath,
    const std::string& entryName,
    std::string& content)
{
    int errorCode = 0;
    zip_t* archive = zip_open(zipPath.c_str(), ZIP_RDONLY, &errorCode);
    if(!archive)
    {
        LOG_ERROR("open zip failed code={} path={}", errorCode, zipPath);
        return false;
    }

    zip_stat_t stat{};
    if(zip_stat(archive, entryName.c_str(), 0, &stat) != 0)
    {
        LOG_ERROR("zip entry not found:{}", entryName);
        zip_close(archive);
        return false;
    }

    zip_file_t* file = zip_fopen(archive, entryName.c_str(), 0);
    if(!file)
    {
        LOG_ERROR("open zip entry failed:{}", entryName);
        zip_close(archive);
        return false;
    }

    content.resize(stat.size);
    zip_int64_t readBytes = zip_fread(file, &content[0], stat.size);

    zip_fclose(file);
    zip_close(archive);

    if(readBytes < 0 || static_cast<zip_uint64_t>(readBytes) != stat.size)
    {
        LOG_ERROR("read zip entry failed:{}", entryName);
        return false;
    }

    return true;
}

bool IdlePointAllocator::parseZones(const std::string& content)
{
    try
    {
        YAML::Node root = YAML::Load(content);
        auto zones = root["zones"];
        if(!zones)
            return true;

        m_idleZoneIds.clear();
        for(auto zone : zones)
        {
            std::string type = zone["type"].as<std::string>("");
            if(type == "IDLE")
            {
                std::string id = zone["id"].as<std::string>("");
                m_idleZoneIds.push_back(id);
            }
        }

        LOG_INFO("idle zones count={}", m_idleZoneIds.size());
        return true;
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("parse zones.yaml failed:{}", e.what());
        return false;
    }
}

bool IdlePointAllocator::parseStations(const std::string& content)
{
    try
    {
        YAML::Node root = YAML::Load(content);
        auto stations = root["stations"];
        if(!stations)
            return true;

        m_points.clear();
        m_occupied.clear();

        for(auto station : stations)
        {
            std::string stationId = station["id"].as<std::string>("");

            auto locations = station["locations"];
            if(!locations)
                continue;

            for(auto location : locations)
            {
                std::string type = location["type"].as<std::string>("");
                if(type != "IDLE")
                    continue;

                IdlePoint point;
                point.station_id = stationId;
                point.location_id = location["id"].as<std::string>("");
                point.pose.x = location["pose"]["x"].as<double>();
                point.pose.y = location["pose"]["y"].as<double>();
                point.pose.yaw = location["pose"]["yaw"].as<double>();

                m_points.push_back(point);
                m_occupied.push_back(false);
            }
        }

        LOG_INFO("idle points loaded count={}", m_points.size());
        return true;
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("parse stations.yaml failed:{}", e.what());
        return false;
    }
}

}
