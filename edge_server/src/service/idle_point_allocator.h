#pragma once

#include <mutex>
#include <string>
#include <vector>
#include "types.h"

namespace edge_server
{

struct IdlePoint
{
    std::string station_id;
    std::string location_id;
    Pose2D pose;
};

class IdlePointAllocator
{
public:
    // 从地图包(zip)中解析 zones.yaml / stations.yaml，加载 IDLE 充电点
    bool load(const std::string& mapPackagePath);

    // 按顺序分配一个未被占用的充电点
    bool allocate(Pose2D& pose);

    // 标记某个点位已被占用（Edge 重启后恢复占用状态）
    void markOccupied(const Pose2D& pose);

    bool empty() const;
    size_t size() const;

private:
    bool extractFile(const std::string& zipPath, const std::string& entryName, std::string& content);
    bool parseZones(const std::string& content);
    bool parseStations(const std::string& content);

private:
    std::vector<IdlePoint> m_points;
    std::vector<bool> m_occupied;
    std::vector<std::string> m_idleZoneIds;
    mutable std::mutex m_mutex;
};

}
