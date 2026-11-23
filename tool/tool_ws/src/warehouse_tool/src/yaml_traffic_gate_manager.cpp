#include "warehouse_tool/yaml_traffic_gate_manager.h"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <utility>

namespace
{
bool readPose(const YAML::Node& node, Pose2D& pose)
{
    if(!node || !node["x"] || !node["y"])
        return false;

    pose.x = node["x"].as<double>();
    pose.y = node["y"].as<double>();
    pose.yaw = node["yaw"] ? node["yaw"].as<double>() : 0.0;
    return true;
}

bool readLane(const YAML::Node& wait_poses, const std::string& key, std::vector<Pose2D>& out)
{
    out.clear();
    if(!wait_poses || !wait_poses[key])
        return true;

    for(const auto& poseNode : wait_poses[key])
    {
        Pose2D pose{};
        if(!readPose(poseNode, pose))
            return false;
        out.push_back(pose);
    }
    return true;
}
}

bool YamlTrafficGateManager::load(const std::string& filename)
{
    m_filename = filename;
    std::ifstream file(filename);
    if(!file.good())
    {
        std::cout << "traffic_gates file not exist, and it will be created subsequently\n";
        m_vGates.clear();
        return true;
    }

    try
    {
        YAML::Node root = YAML::LoadFile(filename);
        auto gates = root["traffic_gates"];
        if(!gates)
        {
            m_vGates.clear();
            return true;
        }

        m_vGates.clear();
        int marker_id = 1;
        for(auto gateNode : gates)
        {
            TrafficGate gate;
            gate.marker_id = marker_id++;
            if(!gateNode["resource_id"])
            {
                std::cerr << "traffic_gate missing resource_id, skip\n";
                continue;
            }
            gate.resource_id = gateNode["resource_id"].as<std::string>();

            auto polygon = gateNode["polygon"];
            if(polygon)
            {
                for(auto point : polygon)
                {
                    if(point.size() < 2)
                        continue;
                    gate.polygon.emplace_back(point[0].as<double>(), point[1].as<double>());
                }
            }

            if(!readLane(gateNode["wait_poses"], "l1", gate.l1_wait_poses) ||
               !readLane(gateNode["wait_poses"], "l2", gate.l2_wait_poses))
            {
                std::cerr << "parse wait_poses failed resource_id=" << gate.resource_id << "\n";
                return false;
            }

            m_vGates.push_back(gate);
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << "load traffic_gates failed:" << e.what() << std::endl;
        return false;
    }

    return true;
}

bool YamlTrafficGateManager::addGate(TrafficGate& gate)
{
    auto iter = std::find_if(m_vGates.begin(), m_vGates.end(), [&](const TrafficGate& item)
            {
                return item.resource_id == gate.resource_id;
            });
    if(iter != m_vGates.end())
    {
        std::cout << "resource_id exists\n";
        return false;
    }

    gate.marker_id = static_cast<int>(m_vGates.size()) + 1;
    m_vGates.push_back(gate);
    return true;
}

bool YamlTrafficGateManager::updateGate(const TrafficGate& gate)
{
    for(auto& item : m_vGates)
    {
        if(item.resource_id == gate.resource_id)
        {
            int marker_id = item.marker_id;
            item = gate;
            item.marker_id = marker_id;
            return true;
        }
    }

    return false;
}

bool YamlTrafficGateManager::removeGate(const std::string& resource_id)
{
    auto iter = std::remove_if(m_vGates.begin(), m_vGates.end(), [&](const TrafficGate& gate)
            {
                return gate.resource_id == resource_id;
            });

    if(iter == m_vGates.end())
        return false;

    m_vGates.erase(iter, m_vGates.end());
    return true;
}

bool YamlTrafficGateManager::save()
{
    if(m_filename.empty())
        return false;

    YAML::Emitter out;
    out << YAML::BeginMap;

    out << YAML::Key << "traffic_gates";
    out << YAML::Value << YAML::BeginSeq;

    for(const auto& gate : m_vGates)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "resource_id";
        out << YAML::Value << gate.resource_id;

        out << YAML::Key << "polygon";
        out << YAML::Value << YAML::BeginSeq;
        for(const auto& p : gate.polygon)
        {
            out << YAML::Flow
                << YAML::BeginSeq
                << p.first
                << p.second
                << YAML::EndSeq;
        }
        out << YAML::EndSeq;

        out << YAML::Key << "wait_poses";
        out << YAML::Value << YAML::BeginMap;
        const std::pair<const char*, const std::vector<Pose2D>*> lanes[] = {
            {"l1", &gate.l1_wait_poses},
            {"l2", &gate.l2_wait_poses}
        };
        for(const auto& lane : lanes)
        {
            out << YAML::Key << lane.first;
            out << YAML::Value << YAML::BeginSeq;
            for(const auto& pose : *lane.second)
            {
                out << YAML::Flow << YAML::BeginMap;
                out << YAML::Key << "x" << YAML::Value << pose.x;
                out << YAML::Key << "y" << YAML::Value << pose.y;
                out << YAML::Key << "yaw" << YAML::Value << pose.yaw;
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;
        }
        out << YAML::EndMap;

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

const std::vector<TrafficGate>& YamlTrafficGateManager::getGates() const
{
    return m_vGates;
}
