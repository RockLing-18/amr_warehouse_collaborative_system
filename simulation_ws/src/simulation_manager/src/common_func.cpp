#include "simulation_manager/common_func.h"

namespace simulation_manager
{

std::string CommonFunc::makeModelName(const std::string& robot_id, const std::string& instance_id)
{
    return "amr_" + robot_id + "_" + instance_id;
}

}