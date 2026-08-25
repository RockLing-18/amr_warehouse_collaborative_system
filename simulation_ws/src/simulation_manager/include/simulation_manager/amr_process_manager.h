#pragma once

#include "simulation_manager/data_define.h"
#include <map>
#include <sys/types.h>

namespace simulation_manager
{

class AmrProcessManager
{
public:
    bool spawn(const RobotInfo& robot);
    bool stop( const std::string& robot_id, const std::string& instance_id);

private:
    struct ProcessInfo
    {
        pid_t pid{-1};
    };

private:
    std::map<std::string, ProcessInfo> m_processes;
};

}