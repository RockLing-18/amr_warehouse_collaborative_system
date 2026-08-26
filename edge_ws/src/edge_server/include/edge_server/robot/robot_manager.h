#pragma once

#include "edge_server/common/types.h"
#include <vector>

namespace edge_server
{

class RobotManager
{
public:
    RobotManager();

    std::vector<RobotInfo> getRobotList();
};


}