#include "robot/robot_list_publisher.h"
#include "robot/robot_manager.h"
#include "topic/topic_manager.h"
#include "nlohmann/json.hpp"
#include "utils/LogDefine.h"

using json = nlohmann::json;

namespace edge_server
{
RobotListPublisher::RobotListPublisher(const std::shared_ptr<RobotManager> &robot_manager, const std::shared_ptr<TopicManager> &topicManager)
: m_robot_manager(robot_manager), m_topicManager(topicManager)
{
}

RobotListPublisher::~RobotListPublisher()
{
    stop();
}


void RobotListPublisher::start(int period_ms)
{
    if(!m_timer)
        m_timer = std::make_unique<Timer>();
    
    m_timer->start(std::chrono::milliseconds(period_ms),
        [this]()
        {
            publish();
        });
}

void RobotListPublisher::stop()
{
    if(m_timer)
    {
        m_timer->stop();
    }
}

void RobotListPublisher::publish()
{
    try
    {
        if(!m_topicManager->hasSubscriber("robot_list"))
        {
            return;
        }

        std::vector<RobotInfo> robots = m_robot_manager->getRobotList();
        json msg;

        msg["msgType"] = "subscribe";
        msg["topic"] = "robot_list";
        msg["robots"] = json::array();
        for(auto& robot : robots)
        {
            json item;
            item["robot_id"] = robot.robot_id;
            item["instance_id"] = robot.instance_id;
            item["x"] = robot.x;
            item["y"] = robot.y;
            item["yaw"] = robot.yaw;
            msg["robots"].push_back(item);
        }

        m_topicManager->publish("robot_list", msg.dump());
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("exception:{}", e.what());
    }
}
}
