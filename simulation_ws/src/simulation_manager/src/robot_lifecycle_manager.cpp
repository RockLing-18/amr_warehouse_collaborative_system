#include "simulation_manager/robot_lifecycle_manager.h"
#include <chrono>

namespace simulation_manager
{

RobotLifecycleManager::RobotLifecycleManager(
    const rclcpp::Node::SharedPtr& node,
    const std::shared_ptr<GazeboClient>& gazebo_client,
    const std::shared_ptr<AmrProcessManager>& process_manager)
: m_node(node), m_gazebo_client(gazebo_client), m_process_manager(process_manager)
{
    m_controller_checker = std::make_shared<ControllerChecker>(node);

    m_worker_thread = std::thread(&RobotLifecycleManager::workerLoop, this);
}

RobotLifecycleManager::~RobotLifecycleManager()
{
    m_running=false;
    m_cv.notify_all();
    if(m_worker_thread.joinable())
        m_worker_thread.join();
}

void RobotLifecycleManager::requestCreate(const RobotInfo& robot)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(m_robots.find(robot.robot_id) != m_robots.end())
    {
        if(it->second.state != RobotState::FAILED)
        {
            RCLCPP_WARN(
                m_node->get_logger(),
                "Robot already managed id=%s state=%d",
                robot.robot_id.c_str(),
                static_cast<int>(t->second.state));

            return;
        }
    }

    RCLCPP_INFO(
        m_node->get_logger(),
        "Queue create robot=%s instance=%s",
        robot.robot_id.c_str(),
        robot.instance_id.c_str());

    ManagedRobot managed;
    managed.info = robot;
    managed.state = RobotState::WAITING_CREATE;
    m_robots[robot.robot_id] = managed;

    LifecycleRequest req;
    req.type = LifecycleRequest::Type::CREATE;
    req.robot = robot;

    m_queue.push(req);
    m_cv.notify_one();
}

void RobotLifecycleManager::requestDelete(const GazeboModelInfo& model)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    LifecycleRequest req;
    req.type = LifecycleRequest::Type::DELETE;
    req.model = model;
    m_queue.push(req);
    m_cv.notify_one();

    RCLCPP_INFO(
        m_node->get_logger(),
        "Queue delete robot=%s instance=%s",
        model.robot_id.c_str(),
        model.instance_id.c_str());
}

void RobotLifecycleManager::requestReplace(const RobotInfo& robot, const GazeboModelInfo& old_model)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    LifecycleRequest req;
    req.type = LifecycleRequest::Type::REPLACE;
    req.robot = robot;
    req.model = old_model;
    m_queue.push(req);
    m_cv.notify_one();

    RCLCPP_INFO(
        m_node->get_logger(),
        "Queue replace robot=%s old=%s new=%s",
        robot.robot_id.c_str(),
        old_model.instance_id.c_str(),
        robot.instance_id.c_str());
}


void RobotLifecycleManager::workerLoop()
{
    while(m_running)
    {
        LifecycleRequest request;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(
                lock,
                [this]
                {
                    return !m_queue.empty() || !m_running;
                });

            if(!m_running && m_queue.empty())
                return;

            request = m_queue.front();
            m_queue.pop();
        }

        processRequest(request);
    }
}

void RobotLifecycleManager::processRequest(const LifecycleRequest& request)
{
    switch(request.type)
    {
        case LifecycleRequest::Type::CREATE:
        {
            bool success = createRobot(request.robot);
            if(!success)
            {
                setState(robot.robot_id, RobotState::FAILED);
            }

            break;
        }
        case LifecycleRequest::Type::DELETE:
        {
            deleteRobot(request.model);
            break;
        }
        case LifecycleRequest::Type::REPLACE:
        {
            replaceRobot(request.robot, request.model);
            break;
        }
    }
}


bool RobotLifecycleManager::createRobot(const RobotInfo& robot)
{
    RCLCPP_INFO(m_node->get_logger(), "Start create robot=%s", robot.robot_id.c_str());

    setState(robot.robot_id, RobotState::LAUNCHING);

    // 启动ros2 launch
    if(!m_process_manager->spawn(robot))
    {
        RCLCPP_ERROR(m_node->get_logger(), "Launch failed robot=%s", robot.robot_id.c_str());
        return false;
    }

    setState(robot.robot_id, RobotState::WAIT_GAZEBO_MODEL);

    // 等待Gazebo model
    if(!waitGazeboModel(robot, std::chrono::seconds(10)))
    {
        RCLCPP_ERROR( m_node->get_logger(), "Gazebo model timeout");
        cleanupRobot(robot);
        return false;
    }

    setState(robot.robot_id, RobotState::WAIT_CONTROLLER);

    // controller active
    if(!waitControllerReady(robot, std::chrono::seconds(30)))
    {
        RCLCPP_ERROR(m_node->get_logger(), "Controller timeout");
        cleanupRobot(robot);
        return false;
    }

    setState(robot.robot_id, RobotState::ACTIVE);

    RCLCPP_INFO(m_node->get_logger(), "Robot active=%s", robot.robot_id.c_str());
    return true;
}

bool RobotLifecycleManager::deleteRobot(const GazeboModelInfo& model)
{
    RCLCPP_INFO(
        m_node->get_logger(),
        "Delete robot=%s instance=%s",
        model.robot_id.c_str(),
        model.instance_id.c_str());

    setState(robot.robot_id, RobotState::DELETING);

    // 1. stop ros2 launch
    m_process_manager->stop(model.robot_id, model.instance_id);

    // 2. delete gazebo model
    m_gazebo_client->deleteModelAsync(
        model.model_name,
        [this,model](bool success)
        {
            if(success)
            {
                RCLCPP_INFO( m_node->get_logger(), "Robot deleted=%s", model.robot_id.c_str());
            }
            else
            {
                RCLCPP_ERROR(m_node->get_logger(), "Delete failed=%s", model.robot_id.c_str());
            }

        });


    std::lock_guard<std::mutex> lock(m_mutex);
    m_robots.erase(model.robot_id);
    return true;
}

bool RobotLifecycleManager::replaceRobot(const RobotInfo& robot, const GazeboModelInfo& old_model)
{
    RCLCPP_INFO(
        m_node->get_logger(),
        "Replace robot=%s old=%s new=%s",
        robot.robot_id.c_str(),
        old_model.instance_id.c_str(),
        robot.instance_id.c_str());

    // stop old process
    m_process_manager->stop(old_model.robot_id, old_model.instance_id);

    // 删除gazebo
    m_gazebo_client->deleteModelAsync(
        old_model.model_name,
        [this, robot](bool success)
        {
            if(!success)
            {
                RCLCPP_ERROR(m_node->get_logger(), "Replace delete failed");
                return;
            }

            // 删除完成后创建新的
            requestCreate(robot);
        });

    return true;
}


bool RobotLifecycleManager::waitGazeboModel(const RobotInfo& robot, std::chrono::seconds timeout)
{
    auto start = std::chrono::steady_clock::now();
    while(rclcpp::ok())
    {
        if(hasGazeboModel(robot))
            return true;

        if(std::chrono::steady_clock::now() - start > timeout)
        {
            RCLCPP_INFO(m_node->get_logger(), "Get robot model timeout robot=%s instance=%s", robot.robot_id.c_str(), robot.instance_id.c_str());
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return false;
}

bool RobotLifecycleManager::waitControllerReady(const RobotInfo& robot, std::chrono::seconds timeout)
{
    std::atomic_bool ready = false;

    m_controller_checker->checkAsync(
        robot.robot_id,
        [&ready](bool result)
        {
            ready = result;
        });

    auto start = std::chrono::steady_clock::now();
    while(rclcpp::ok())
    {
        if(ready)
            return true;

        if(std::chrono::steady_clock::now() - start > timeout)
        {
            RCLCPP_INFO(m_node->get_logger(), "Get robot controller_manager status timeout robot=%s instance=%s", robot.robot_id.c_str(), robot.instance_id.c_str());
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    return false;
}

void RobotLifecycleManager::cleanupRobot( const RobotInfo& robot)
{
    RCLCPP_WARN( m_node->get_logger(), "Cleanup robot=%s", robot.robot_id.c_str());

    m_process_manager->stop(robot.robot_id, robot.instance_id);

    GazeboModelInfo model;
    model.model_name = CommonFunc::makeModelName(robot.robot_id, robot.instance_id);

    m_gazebo_client->deleteModelAsync(model.model_name, [](bool success){});
}

void RobotLifecycleManager::setState(const std::string& robot_id, RobotState state)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it=m_robots.find(robot_id);
    if(it!=m_robots.end())
    {
        it->second.state=state;
    }
}


}
