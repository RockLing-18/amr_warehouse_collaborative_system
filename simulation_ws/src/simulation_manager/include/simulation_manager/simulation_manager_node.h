#pragma once
#include <rclcpp/rclcpp.hpp>

namespace simulation_manager
{

class SimulationManagerNode : public rclcpp::Node
{
public:
    explicit SimulationManagerNode();

private:
    void onTimer();

private:
    rclcpp::TimerBase::SharedPtr m_timer;
};

}