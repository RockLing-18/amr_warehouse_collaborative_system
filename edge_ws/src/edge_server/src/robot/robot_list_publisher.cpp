#include "edge_server/robot/robot_list_publisher.h"
#include "edge_server/common/json.hpp"
#include <iostream>

using json = nlohmann::json;

namespace edge_server
{
RobotListPublisher::RobotListPublisher(const std::shared_ptr<RobotManager> &robot_manager, const std::shared_ptr<WebSocketServer> &websocket)
: m_robot_manager(robot_manager), m_websocket(websocket)
{
}

RobotListPublisher::~RobotListPublisher()
{
    stop();
}


void RobotListPublisher::start(int period_ms)
{
    m_period_ms = period_ms;
    m_running = true;
    m_thread = std::thread(
            [this]()
            {
                while(m_running)
                {
                    publish();
                    std::this_thread::sleep_for(std::chrono::milliseconds(m_period_ms));
                }
            });
}

void RobotListPublisher::stop()
{
    m_running = false;
    if(m_thread.joinable())
        m_thread.join();
}

void RobotListPublisher::publish()
{
    try
    {
        std::vector<RobotInfo> robots = m_robot_manager->getRobotList();
        json msg;

        msg["type"]="robot_list";
        msg["robots"]=json::array();
        for(auto& robot:robots)
        {
            json item;
            item["robot_id"] = robot.robot_id;
            item["instance_id"] = robot.instance_id;
            item["x"] = robot.x;
            item["y"] = robot.y;
            item["yaw"] = robot.yaw;
            msg["robots"].push_back(item);
        }

        m_websocket->broadcast(msg.dump());
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what();
    }
}
}
