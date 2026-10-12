#include "amr_behavior_manager/goods_point_resolver.h"

#include <algorithm>

namespace amr_behavior_manager
{

GoodsPointResolver::GoodsPointResolver(std::shared_ptr<ZoneStationLoader> loader)
    : m_loader(loader)
{
}

bool GoodsPointResolver::resolve(
    const std::string& goods_id,
    const std::string& task_type,
    LocationInfo& location) const
{
    if(!m_loader)
        return false;

    const std::string target_zone_type =
        task_type == "LOAD" ? "load" : "unload";

    const auto& zones = m_loader->getZones();
    auto zoneIt = std::find_if(
        zones.begin(),
        zones.end(),
        [&goods_id, &target_zone_type](const ZoneInfo& zone)
        {
            if(zone.type != target_zone_type)
                return false;
            return std::find(zone.goods_ids.begin(), zone.goods_ids.end(), goods_id) != zone.goods_ids.end();
        });

    if(zoneIt == zones.end())
        return false;

    const auto& stations = m_loader->getStations();
    auto stationIt = std::find_if(
        stations.begin(),
        stations.end(),
        [&zoneIt](const StationInfo& station)
        {
            return station.zone_id == zoneIt->id;
        });

    if(stationIt == stations.end() || stationIt->locations.empty())
        return false;

    auto locationIt = std::find_if(
        stationIt->locations.begin(),
        stationIt->locations.end(),
        [&target_zone_type](const LocationInfo& item)
        {
            return item.type == target_zone_type;
        });

    if(locationIt == stationIt->locations.end())
        locationIt = stationIt->locations.begin();

    location = *locationIt;
    return true;
}

}
