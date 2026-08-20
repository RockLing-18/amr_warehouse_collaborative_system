#pragma once
#include <string>
#include <vector>
#include "data_define.h"

class YamlWallManager
{
public:
    bool load(const std::string& filename);
    bool addWall(Wall& wall);
    bool updateWall(const Wall& wall);
    bool removeWall(const std::string& id);
    bool save();
    const std::vector<Wall>& getWalls() const;

private:
    std::string m_filename;
    std::vector<Wall> m_vWall;
};