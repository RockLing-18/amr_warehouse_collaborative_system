#pragma once
#include <string>
#include <vector>
#include "data_define.h"

class YamlZoneManager
{
public:
    bool load(const std::string& filename);
    bool addZone(Zone& zone);
    bool updateZone(const Zone& zone);
    bool removeZone(const std::string& id);
    bool save();
    const std::vector<Zone>& getZones() const;

private:
    std::string m_filename;
    std::vector<Zone> m_vZone;
};