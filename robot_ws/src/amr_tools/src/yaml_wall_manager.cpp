#include "amr_tools/yaml_wall_manager.h"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iostream>
#include <algorithm>

bool YamlWallManager::load(const std::string& filename)
{
    m_filename = filename;
    std::ifstream file(filename);
    if(!file.good())
    {
        std::cout <<"wall file not exist, and it will be created subsequently\n";
        return true;
    }

    try
    {
        YAML::Node root = YAML::LoadFile(filename);
        auto walls = root["walls"];

        if(!walls)
        {
            return true;
        }

        m_vWall.clear();
        int marker_id = 1;
        for(auto wallNode : walls)
        {
            Wall wall;
            wall.id = wallNode["id"].as<std::string>();
            wall.marker_id = marker_id++;
            auto line = wallNode["line"];
            for(auto point : line)
            {
                double x = point[0].as<double>();
                double y = point[1].as<double>();
                wall.line.emplace_back(x, y);
            }

            m_vWall.push_back(wall);
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << "load walls failed:" << e.what() << std::endl;
        return false;
    }

    return true;
}

bool YamlWallManager::addWall(Wall& wall)
{
    for(auto& item : m_vWall)
    {
        if(item.id == wall.id)
        {
            std::cout << "wall id exists\n";
            return false;
        }
    }

    wall.marker_id = m_vWall.size() + 1;

    m_vWall.push_back(wall);
    return true;
}

bool YamlWallManager::updateWall(const Wall& wall)
{
    for(auto& item : m_vWall)
    {
        if(item.id == wall.id)
        {
            item = wall;
            return true;
        }
    }

    return false;
}

bool YamlWallManager::removeWall(const std::string& id)
{
    auto iter = std::remove_if(m_vWall.begin(), m_vWall.end(), [&](const Wall& wall)
            {
                return wall.id == id;

            });

    if(iter == m_vWall.end())
        return false;

    m_vWall.erase(iter, m_vWall.end());
    return true;
}

bool YamlWallManager::save()
{
    if(m_filename.empty())
        return false;

    YAML::Emitter out;
    out << YAML::BeginMap;

    out << YAML::Key << "walls";
    out << YAML::Value << YAML::BeginSeq;

    for(auto& wall : m_vWall)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "id";
        out << YAML::Value << wall.id;

        out << YAML::Key << "line";
        out << YAML::Value << YAML::BeginSeq;
        for(auto& p:wall.line)
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

const std::vector<Wall>& YamlWallManager::getWalls() const
{
    return m_vWall;
}