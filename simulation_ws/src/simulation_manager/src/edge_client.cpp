#include "simulation_manager/edge_client.h"
#include "json.hpp"
#include <iostream>

namespace simulation_manager
{
using json=nlohmann::json;

EdgeClient::EdgeClient(const std::string &websocket_url)
: m_websocket_url(websocket_url)
{
}

EdgeClient::~EdgeClient()
{
    m_running=false;

    m_cv.notify_all();

    if(m_thread.joinable())
        m_thread.join();
}

bool EdgeClient::connect()
{
    m_thread = std::thread(&EdgeClient::notifyDealThread, this);

    m_ws.setMessageCallback(
        std::bind(
            &EdgeClient::onMessage,
            this,
            std::placeholders::_1));
    
   return m_ws.connect(m_websocket_url);
}

void EdgeClient::setNotifyCallback(NotifyCallback callback)
{
    m_notify_callback = callback;
}

bool EdgeClient::isConnected() const
{
    return m_ws.isConnected();
}

std::vector<RobotInfo> EdgeClient::getRobotList()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_robotList;
}

void EdgeClient::notifyDealThread()
{
    while(m_running)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(
            lock,
            [this]
            {
                return !m_running || m_robotListUpdated;
            });

    if(!m_running)
        break;

    if(!m_robotListUpdated)
        continue;

    m_robotListUpdated=false;
    lock.unlock();
    
    if(m_notify_callback)
        m_notify_callback();
    }
}

void EdgeClient::onMessage(const std::string& message)
{
    try
    {
        auto j =json::parse(message);

        if(!j.contains("type"))
            return;

        if(j["type"]=="robot_list")
        {
            handleRobotList(message);
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what();
    }
}

void EdgeClient::handleRobotList(const std::string& message)
{
    try
    {
        std::vector<RobotInfo> robots;

        auto j = json::parse(message);
        for(auto& item : j["robots"])
        {
            RobotInfo robot;
            robot.robot_id = item["robot_id"];
            robot.instance_id = item["instance_id"];
            robot.x = item["x"];
            robot.y = item["y"];
            robot.yaw = item["yaw"];
            robots.push_back(robot);
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_robotList = robots;
        m_robotListUpdated = true;
        m_cv.notify_one();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what();
    }
    

    // std::vector<RobotInfo> robots = {
    //     {
    //         "robot01",
    //         "A001",
    //         2.0,
    //         3.0,
    //         1.57
    //     },
    //     {
    //         "robot02",
    //         "B001",
    //         0.0,
    //         0.0,
    //         0.0
    //     },
    //     {
    //         "robot03",
    //         "C001",
    //         0.0,
    //         1.0,
    //         0.0
    //     }
    // };
}

}