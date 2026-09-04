#pragma once
#include "types.h"

namespace edge_server
{

class ConfigLoader
{
public:
    static bool load(const std::string& file, Config& config);
};


}