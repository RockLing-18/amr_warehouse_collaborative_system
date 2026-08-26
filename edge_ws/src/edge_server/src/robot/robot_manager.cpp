#include "edge_server/robot/robot_manager.h"

namespace edge_server
{
RobotManager::RobotManager()
{
}

std::vector<RobotInfo> RobotManager::getRobotList()
{
    std::vector<RobotInfo> robots = {
        {
            "robot01",
            "A001",
            2.0,
            3.0,
            1.57
        },
        {
            "robot02",
            "B001",
            0.0,
            0.0,
            0.0
        },
        {
            "robot03",
            "C001",
            0.0,
            1.0,
            0.0
        }
    };

    return robots;
}

}