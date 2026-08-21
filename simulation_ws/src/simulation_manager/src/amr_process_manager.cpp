#include "simulation_manager/amr_process_manager.h"

#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

namespace simulation_manager
{

std::string AmrProcessManager::makeKey( const std::string& robot_id, const std::string& instance_id) const
{
    return robot_id + ":" + instance_id;
}

bool AmrProcessManager::spawn(const RobotInfo& robot)
{
    const auto key = makeKey(robot.robot_id, robot.instance_id);
    if (m_processes.find(key) != m_processes.end())
        return false;
    
    const pid_t pid = fork();
    if (pid < 0)
        return false;

    if (pid == 0)
    {
        const std::string x = std::to_string(robot.x);
        const std::string y = std::to_string(robot.y);
        const std::string yaw = std::to_string(robot.yaw);

        execlp(
            "ros2",
            "ros2",
            "launch",
            "amr_description",
            "spawn_amr.launch.py",
            ("robot_id:=" + robot.robot_id).c_str(),
            ("instance_id:=" + robot.instance_id).c_str(),
            ("x:=" + x).c_str(),
            ("y:=" + y).c_str(),
            ("yaw:=" + yaw).c_str(),
            static_cast<char*>(nullptr));

        // exec 失败
        _exit(127);
    }

    m_processes[key] = {pid};
    return true;
}

bool AmrProcessManager::stop(const std::string& robot_id, const std::string& instance_id)
{
    const auto key = makeKey(robot_id, instance_id);
    auto it = m_processes.find(key);
    if (it == m_processes.end())
        return true;

    const pid_t pid = it->second.pid;
    if (kill(pid, SIGINT) != 0)
    {
        m_processes.erase(it);
        return false;
    }

    m_processes.erase(it);
    return true;
}


}