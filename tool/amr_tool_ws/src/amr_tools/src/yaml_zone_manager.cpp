#include "amr_tools/yaml_zone_manager.h"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iostream>
#include <algorithm>

bool YamlZoneManager::load(const std::string& filename)
{
    m_filename = filename;
    std::ifstream file(filename);
    if(!file.good())
    {
        std::cout <<"zone file not exist, and it will be created subsequently\n";
        return true;
    }

    try
    {
        YAML::Node root = YAML::LoadFile(filename);
        auto zones = root["zones"];

        if(!zones)
        {
            return true;
        }

        m_vZone.clear();
        int marker_id = 1;
        for(auto zoneNode : zones)
        {
            Zone zone;
            zone.info.id = zoneNode["id"].as<std::string>();
            zone.marker_id = marker_id++;
            zone.info.name = zoneNode["name"].as<std::string>();
            zone.info.type = zoneNode["type"].as<std::string>();
            auto polygon = zoneNode["polygon"];
            for(auto point : polygon)
            {
                double x = point[0].as<double>();
                double y = point[1].as<double>();
                zone.polygon.emplace_back(x, y);
            }

            m_vZone.push_back(zone);
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << "load zones failed:" << e.what() << std::endl;
        return false;
    }

    return true;
}

bool YamlZoneManager::addZone(Zone& zone)
{
    for(auto& item : m_vZone)
    {
        if(item.info.id == zone.info.id)
        {
            std::cout << "zone id exists\n";
            return false;
        }
    }

    zone.marker_id = m_vZone.size() + 1;

    m_vZone.push_back(zone);
    return true;
}

bool YamlZoneManager::updateZone(const Zone& zone)
{
    for(auto& item : m_vZone)
    {
        if(item.info.id == zone.info.id)
        {
            item = zone;
            return true;
        }
    }

    return false;
}

bool YamlZoneManager::removeZone(const std::string& id)
{
    auto iter = std::remove_if(m_vZone.begin(), m_vZone.end(), [&](const Zone& zone)
            {
                return zone.info.id == id;

            });

    if(iter == m_vZone.end())
        return false;

    m_vZone.erase(iter, m_vZone.end());
    return true;
}

bool YamlZoneManager::save()
{
    if(m_filename.empty())
        return false;

    YAML::Emitter out;
    out << YAML::BeginMap;

    out << YAML::Key << "zones";
    out << YAML::Value << YAML::BeginSeq;

    for(auto& zone : m_vZone)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "id";
        out << YAML::Value << zone.info.id;

        out << YAML::Key << "name";
        out << YAML::Value << zone.info.name;

        out << YAML::Key << "type";
        out << YAML::Value << zone.info.type;

        out << YAML::Key << "polygon";
        out << YAML::Value << YAML::BeginSeq;
        for(auto& p:zone.polygon)
        {
            out << YAML::Flow
                << YAML::BeginSeq
                << p.first
                << p.second
                << YAML::EndSeq;
        }

        out << YAML::EndSeq;
        out << YAML::EndMap;
    }


    out << YAML::EndSeq;
    out << YAML::EndMap;

    std::ofstream file(m_filename);
    if(!file.is_open())
    {
        std::cout << "open file failed\n";
        return false;
    }

    file << out.c_str();
    file.close();
    return true;
}

const std::vector<Zone>& YamlZoneManager::getZones() const
{
    return m_vZone;
}