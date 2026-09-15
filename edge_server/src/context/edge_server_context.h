#pragma once

#include <memory>

namespace edge_server
{

class SQLiteDB;

class RobotManager;
class TaskManager;


class MapService;
class RobotService;
class TaskService;


class EdgeServerContext
{

public:

    EdgeServerContext();


    bool init();


public:

    std::shared_ptr<RobotManager>
    robotManager()
    {
        return m_robotManager;
    }


    std::shared_ptr<MapService>
    mapService()
    {
        return m_mapService;
    }


    std::shared_ptr<TaskService>
    taskService()
    {
        return m_taskService;
    }


    std::shared_ptr<RobotService>
    robotService()
    {
        return m_robotService;
    }


private:

    std::shared_ptr<RobotManager>
        m_robotManager;


    std::shared_ptr<MapService>
        m_mapService;


    std::shared_ptr<TaskService>
        m_taskService;


    std::shared_ptr<RobotService>
        m_robotService;

};

}