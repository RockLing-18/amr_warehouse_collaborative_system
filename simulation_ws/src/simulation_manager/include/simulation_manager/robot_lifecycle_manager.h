#pragma once

#include <rclcpp/rclcpp.hpp>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <functional>

#include "simulation_manager/amr_process_manager.h"
#include "simulation_manager/gazebo_client.h"
#include "simulation_manager/controller_checker.h"
#include "simulation_manager/data_define.h"


namespace simulation_manager
{

struct LifecycleRequest
{
    enum class Type
    {
        CREATE,
        DELETE,
        REPLACE
    };

    Type type;
    RobotInfo robot;
    GazeboModelInfo model;
};


class RobotLifecycleManager
{
public:
    RobotLifecycleManager(
        const rclcpp::Node::SharedPtr& node,
        const std::shared_ptr<GazeboClient>& gazebo_client,
        const std::shared_ptr<AmrProcessManager>& process_manager);

    ~RobotLifecycleManager();

public:
    void requestCreate(const RobotInfo& robot);
    void requestDelete(const GazeboModelInfo& model);
    void requestReplace( const RobotInfo& robot, const GazeboModelInfo& old_model);

private:
    void processRequest(const LifecycleRequest& request);
    bool deleteRobot(const GazeboModelInfo& model);
    bool replaceRobot(const RobotInfo& robot, const GazeboModelInfo& old_model);


    /*
     * worker线程
     *
     * 保证Gazebo同时只有一个创建流程
     */
    void workerLoop();

    /*
     * 执行完整创建流程
     */
    bool createRobot(const RobotInfo& robot);

    /*
     * 等待Gazebo出现model
     */
    bool waitGazeboModel(const RobotInfo& robot, std::chrono::seconds timeout);

    /*
     * 等待controller active
     */
    bool waitControllerReady(const RobotInfo& robot, std::chrono::seconds timeout);

    /*
     * 删除异常机器人
     */
    void cleanupRobot(const RobotInfo& robot);

    /*
     * 查询Gazebo model
     */
    bool hasGazeboModel(const RobotInfo& robot);

    void setState(const std::string& robot_id, RobotState state)

private:
    rclcpp::Node::SharedPtr m_node;
    std::shared_ptr<GazeboClient> m_gazebo_client;
    std::shared_ptr<AmrProcessManager> m_process_manager;
    std::shared_ptr<ControllerChecker> m_controller_checker;
    std::queue<LifecycleRequest> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic_bool m_running{true};
    std::thread m_worker_thread;

    std::unordered_map<std::string, ManagedRobot> m_robots; // key: robot_id
};

}
