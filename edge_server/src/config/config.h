#pragma once
#include "types.h"

namespace edge_server
{

class ConfigManager
{
public:
    bool load(const std::string& file);
    Config getConfig() const;
private:
    Config m_config;
};


}