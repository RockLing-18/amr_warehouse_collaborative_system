#include "amr_tools/yaml_station_manager.h"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iostream>
#include <algorithm>

bool YamlStationManager::load(const std::string& filename)
{
    m_filename = filename;
    std::ifstream file(filename);
    if(!file.good())
    {
        std::cout <<"station file not exist, and it will be created subsequently\n";
        return true;
    }

    try
    {
        YAML::Node root =YAML::LoadFile(filename);
        auto stations = root["stations"];
        if(!stations)
            return true;

        m_vStation.clear();
        for(auto stationNode:stations)
        {
            Station station;
            station.id = stationNode["id"].as<std::string>();
            station.name = stationNode["name"].as<std::string>();
            station.type = stationNode["type"].as<std::string>();
            station.zone_id = stationNode["zone_id"].as<std::string>();

            auto locations = stationNode["locations"];
            for(auto locationNode : locations)
            {
                Location location;
                location.id = locationNode["id"].as<std::string>();
                location.type = locationNode["type"].as<std::string>();
                location.marker_id = m_marker_id++;

                auto pose = locationNode["pose"];
                location.pose.x = pose["x"].as<double>();
                location.pose.y = pose["y"].as<double>();
                location.pose.yaw = pose["yaw"].as<double>();
                station.locations.push_back(location);
            }

            m_vStation.push_back(station);
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << "load stations failed:" << e.what() << std::endl;
        return false;
    }

    return true;
}

bool YamlStationManager::addStation(Station& station)
{
    for(auto& item : m_vStation)
    {
        if(item.id == station.id)
        {
            std::cout << "Station id exists\n";
            return false;
        }
    }

    for(auto &locationNode : station.locations)
        locationNode.marker_id = m_marker_id++;

    m_vStation.push_back(station);
    return true;
}

bool YamlStationManager::updateStation(const Station& station)
{
    for(auto& item : m_vStation)
    {
        if(item.id == station.id)
        {
            item = station;
            return true;
        }
    }

    return false;
}

bool YamlStationManager::removeStation(const std::string& id)
{
    auto iter = std::remove_if(m_vStation.begin(), m_vStation.end(), [&](const Station& Station)
            {
                return Station.id == id;
            });

    if(iter == m_vStation.end())
        return false;

    m_vStation.erase(iter, m_vStation.end());
    return true;
}

bool YamlStationManager::save()
{
    if(m_filename.empty())
        return false;
    
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "stations";
    out << YAML::Value << YAML::BeginSeq;

    for(auto& station : m_vStation)
    {
        out << YAML::BeginMap;
        out << YAML::Key
            << "id"
            << YAML::Value
            << station.id;

        out << YAML::Key
            << "name"
            << YAML::Value
            << station.name;

        out << YAML::Key
            << "type"
            << YAML::Value
            << station.type;

        out << YAML::Key
            << "zone_id"
            << YAML::Value
            << station.zone_id;

        out << YAML::Key
            << "locations";

        out << YAML::Value
            << YAML::BeginSeq;
        
        for(auto& location : station.locations)
        {
            out << YAML::BeginMap;

            out << YAML::Key
                << "id"
                << YAML::Value
                << location.id;

            out << YAML::Key
                << "type"
                << YAML::Value
                << location.type;

            out << YAML::Key
                << "pose";

            out << YAML::Value
                << YAML::BeginMap;

            out << YAML::Key
                << "x"
                << YAML::Value
                << location.pose.x;

            out << YAML::Key
                << "y"
                << YAML::Value
                << location.pose.y;

            out << YAML::Key
                << "yaw"
                << YAML::Value
                << location.pose.yaw;

            out << YAML::EndMap;
            out << YAML::EndMap;
        }

        out << YAML::EndSeq;
        out << YAML::EndMap;
    }

    out << YAML::EndSeq;
    out << YAML::EndMap;

    std::ofstream file(m_filename);
    if(!file.is_open())
        return false;

    file << out.c_str();
    return true;
}

const std::vector<Station>& YamlStationManager::getStations() const
{
    return m_vStation;
}