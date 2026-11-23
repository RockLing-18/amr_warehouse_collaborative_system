#pragma once
#include <string>
#include <vector>
#include "data_define.h"

class YamlTrafficGateManager
{
public:
    bool load(const std::string& filename);
    bool addGate(TrafficGate& gate);
    bool updateGate(const TrafficGate& gate);
    bool removeGate(const std::string& resource_id);
    bool save();
    const std::vector<TrafficGate>& getGates() const;

private:
    std::string m_filename;
    std::vector<TrafficGate> m_vGates;
};
