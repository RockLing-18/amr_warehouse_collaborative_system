#include "amr_behavior_manager/traffic_gate_loader.h"

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iostream>

namespace amr_behavior_manager
{

bool TrafficGateLoader::load(const std::string& filename)
{
    m_gates.clear();
    if(filename.empty())
        return true;

    std::ifstream file(filename);
    if(!file.good())
    {
        std::cerr << "traffic_gates file not exist:" << filename << "\n";
        return false;
    }

    try
    {
        YAML::Node root = YAML::LoadFile(filename);
        auto gates = root["traffic_gates"];
        if(!gates)
            return true;

        for(const auto& gateNode : gates)
        {
            TrafficGateData gate;
            gate.resource_id = gateNode["resource_id"].as<std::string>();

            auto polygon = gateNode["polygon"];
            for(const auto& point : polygon)
            {
                if(point.size() < 2)
                    continue;
                gate.polygon.emplace_back(point[0].as<double>(), point[1].as<double>());
            }

            auto wait_poses = gateNode["wait_poses"];
            for(const auto& lane : {"l1", "l2"})
            {
                if(!wait_poses || !wait_poses[lane])
                    continue;
                for(const auto& poseNode : wait_poses[lane])
                {
                    TrafficGatePose pose;
                    pose.x = poseNode["x"].as<double>();
                    pose.y = poseNode["y"].as<double>();
                    pose.yaw = poseNode["yaw"].as<double>();
                    gate.wait_poses[lane].push_back(pose);
                }
            }

            m_gates.push_back(gate);
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << "load traffic_gates failed:" << e.what() << "\n";
        m_gates.clear();
        return false;
    }

    return true;
}

const std::vector<TrafficGateData>& TrafficGateLoader::gates() const
{
    return m_gates;
}

}
