#pragma once
#include <string>
#include <vector>
#include "data_define.h"

class YamlStationManager
{
public:
    bool load(const std::string& filename);
    bool save();
    bool addStation(Station& station);
    bool updateStation(const Station& station);
    bool removeStation(const std::string& id);
    const std::vector<Station>& getStations() const;

private:
    std::string m_filename;
    int m_marker_id{0};
    std::vector<Station> m_vStation;
};