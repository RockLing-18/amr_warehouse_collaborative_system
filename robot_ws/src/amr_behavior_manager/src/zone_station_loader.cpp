#include "amr_behavior_manager/zone_station_loader.h"

#include <yaml-cpp/yaml.h>
#include <iostream>

namespace amr_behavior_manager
{

bool ZoneStationLoader::load(const std::string& zone_file, const std::string& station_file)
{
    m_zones.clear();
    m_stations.clear();

    try
    {
        YAML::Node zonesRoot = YAML::LoadFile(zone_file);
        if(zonesRoot["zones"])
        {
            for(const auto& node : zonesRoot["zones"])
            {
                ZoneInfo zone;
                zone.id = node["id"].as<std::string>();
                zone.name = node["name"].as<std::string>();
                zone.type = node["type"].as<std::string>();
                if(node["goods_ids"])
                {
                    for(const auto& goods : node["goods_ids"])
                        zone.goods_ids.push_back(goods.as<std::string>());
                }
                m_zones.push_back(zone);
            }
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << "load zones failed:" << e.what() << std::endl;
        return false;
    }

    try
    {
        YAML::Node stationsRoot = YAML::LoadFile(station_file);
        if(stationsRoot["stations"])
        {
            for(const auto& node : stationsRoot["stations"])
            {
                StationInfo station;
                station.id = node["id"].as<std::string>();
                station.name = node["name"].as<std::string>();
                station.zone_id = node["zone_id"].as<std::string>();
                if(node["locations"])
                {
                    for(const auto& locationNode : node["locations"])
                    {
                        LocationInfo location;
                        location.id = locationNode["id"].as<std::string>();
                        location.type = locationNode["type"].as<std::string>();
                        location.x = locationNode["pose"]["x"].as<double>();
                        location.y = locationNode["pose"]["y"].as<double>();
                        location.yaw = locationNode["pose"]["yaw"].as<double>();
                        station.locations.push_back(location);
                    }
                }
                m_stations.push_back(station);
            }
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << "load stations failed:" << e.what() << std::endl;
        return false;
    }

    return true;
}

const std::vector<ZoneInfo>& ZoneStationLoader::getZones() const
{
    return m_zones;
}

const std::vector<StationInfo>& ZoneStationLoader::getStations() const
{
    return m_stations;
}

}
