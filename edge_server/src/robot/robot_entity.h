#pragma once
#include "types.h"

namespace edge_server
{

class RobotEntity
{
public:
    std::string robotId() const
    {
        return m_info.robot_id;
    }

    RobotInstanceInfo getInstanceInfo() const
    {
        RobotInstanceInfo info;
        info.robot_id = m_info.robot_id;
        info.simulation_instance_id = m_info.simulation_instance_id;
        info.pose = m_status.pose;
        return info;
    }

    void updateBaseInfo(RobotBaseInfo info)
    {
        m_info = info;
    }

    RobotRunningStatus getRunningStatus() const
    {
        return m_status;
    }

    void updateStatus(RobotRunningStatus status)
    {
        m_status = status;
    }

private:
    RobotBaseInfo m_info;
    RobotRunningStatus m_status;
};

}