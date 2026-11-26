#pragma once

#include <map>
#include <string>
#include <vector>

namespace amr_behavior_manager
{

struct TrafficGatePose
{
    double x = 0.0;
    double y = 0.0;
    double yaw = 0.0;
};

struct TrafficGateData
{
    std::string resource_id;
    std::vector<std::pair<double,double>> polygon;
    std::map<std::string, std::vector<TrafficGatePose>> wait_poses;
};

class TrafficGateLoader
{
public:
    bool load(const std::string& filename);
    const std::vector<TrafficGateData>& gates() const;

private:
    std::vector<TrafficGateData> m_gates;
};

}
