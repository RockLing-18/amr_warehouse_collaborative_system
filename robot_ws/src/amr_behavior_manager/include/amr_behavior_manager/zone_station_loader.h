#pragma once

#include <string>
#include <vector>

namespace amr_behavior_manager
{

struct LocationInfo
{
    std::string id;
    std::string type;
    double x = 0.0;
    double y = 0.0;
    double yaw = 0.0;
};

struct StationInfo
{
    std::string id;
    std::string name;
    std::string zone_id;
    std::vector<LocationInfo> locations;
};

struct ZoneInfo
{
    std::string id;
    std::string name;
    std::string type;
    std::vector<std::string> goods_ids;
};

class ZoneStationLoader
{
public:
    bool load(const std::string& zone_file, const std::string& station_file);

    const std::vector<ZoneInfo>& getZones() const;
    const std::vector<StationInfo>& getStations() const;

private:
    std::vector<ZoneInfo> m_zones;
    std::vector<StationInfo> m_stations;
};

}
