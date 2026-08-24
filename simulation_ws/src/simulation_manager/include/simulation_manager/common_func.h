#pragma once 
#include "simulation_manager/data_define.h"

namespace simulation_manager
{

class CommonFunc
{
public:
    static std::string makeModelName(const std::string& robot_id, const std::string& instance_id);
};
}