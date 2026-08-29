#include "simulation_manager/edge_client.h"
#include "json.hpp"
#include <iostream>

namespace simulation_manager
{
using json = nlohmann::json;

EdgeClient::EdgeClient(const std::string &websocket_url)
: m_websocket_url(websocket_url)
{
}

EdgeClient::~EdgeClient()
{
    m_running=false;

    m_cv.notify_all();

    if(m_connection_thread.joinable())
        m_connection_thread.join();

    if(m_notify_thread.joinable())
        m_notify_thread.join();
    
     m_ws.close();
}

bool EdgeClient::connect()
{
    m_running = true;
    m_notify_thread = std::thread(&EdgeClient::notifyDealThread, this);

    // websocket管理线程
    m_connection_thread = std::thread(&EdgeClient::connectionThread, this);

    m_ws.setEventCallback(
        [this](const WebSocketClient::WebSocketEvent& event)
        {
            onEventCallback(event);
        });
    
     if(!m_ws.connect(m_websocket_url))
    {
        return false;
    }

    return subscribe();
}

bool EdgeClient::subscribe()
{
    json msg;
    msg["type"] = "subscribe";
    msg["topics"] = {
        "robot_list"
    };

    return m_ws.send(msg.dump());
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

void EdgeClient::connectionThread()
{
    while(m_running)
    {
        if(!m_ws.isConnected())
        {
            std::cout <<"connecting edge server..." <<std::endl;
            if(m_ws.connect(m_websocket_url))
            {
                std::cout<<"edge connected" <<std::endl;
            }
            else
            {
                std::cerr <<"edge connect failed" <<std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

void EdgeClient::onEventCallback(const WebSocketClient::WebSocketEvent& event)
{
    switch (event.type)
    {
    case WebSocketClient::EventType::CONNECTED:
    {
        std::cout << " edge server connected !" <<std::endl;
        subscribe();
        break;
    }
    case WebSocketClient::EventType::DISCONNECTED:
    {
        std::cout << " edge server disconnected !" <<std::endl;
        break;
    }
    case WebSocketClient::EventType::MESSAGE:
        handleMessage(event.message);
        break;
    case WebSocketClient::EventType::ERROR:
    {
        std::cout << " edge server Error !" <<std::endl;
        break;
    }
    default:
        break;
    }
}

void EdgeClient::handleMessage(const std::string& message)
{
    try
    {
        auto j = json::parse(message);

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