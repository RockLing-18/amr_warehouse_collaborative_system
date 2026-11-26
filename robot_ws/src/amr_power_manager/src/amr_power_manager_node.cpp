#include <algorithm>

#include "rclcpp/rclcpp.hpp"
#include "amr_interfaces/msg/power_mode.hpp"
#include "amr_interfaces/msg/battery_state.hpp"
#include "amr_common/ros_interface_names.h"

namespace amr_power_manager
{

class AmrPowerManagerNode : public rclcpp::Node
{
public:
    using PowerMode = amr_interfaces::msg::PowerMode;
    using BatteryState = amr_interfaces::msg::BatteryState;

    AmrPowerManagerNode()
        : Node("amr_power_manager")
    {
        this->declare_parameter<double>("idle_drain_pct_per_min", 0.2);
        this->declare_parameter<double>("work_drain_pct_per_min", 2.0);
        this->declare_parameter<double>("charge_pct_per_min", 10.0);
        this->declare_parameter<double>("low_soc_threshold", 20.0);
        this->declare_parameter<double>("charge_target_soc", 80.0);
        this->declare_parameter<double>("mode_timeout_sec", 3.0);

        m_power_mode_sub = this->create_subscription<PowerMode>(
            amr_common::topic::POWER_MODE,
            10,
            [this](const PowerMode::SharedPtr msg)
            {
                if(!msg)
                    return;
                m_mode = msg->mode;
                m_last_mode_stamp = this->now();
            });

        m_power_level_pub = this->create_publisher<BatteryState>(
            amr_common::topic::POWER_LEVEL, 10);

        m_last_tick = this->now();
        m_tick_timer = this->create_wall_timer(
            std::chrono::milliseconds(100),
            [this]()
            {
                tick();
            });
    }

private:
    void tick()
    {
        auto now = this->now();
        double dt_sim = (now - m_last_tick).seconds();
        m_last_tick = now;
        if(dt_sim < 0.0 || dt_sim > 1.0)
            dt_sim = 0.0;

        if((now - m_last_mode_stamp).seconds() >
           this->get_parameter("mode_timeout_sec").as_double())
        {
            m_mode = PowerMode::MODE_IDLE;
        }

        double rate = 0.0;
        if(m_mode == PowerMode::MODE_CHARGING)
            rate = this->get_parameter("charge_pct_per_min").as_double();
        else if(m_mode == PowerMode::MODE_WORK)
            rate = -this->get_parameter("work_drain_pct_per_min").as_double();
        else
            rate = -this->get_parameter("idle_drain_pct_per_min").as_double();

        m_soc = std::clamp(m_soc + rate * dt_sim / 60.0, 0.0, 100.0);

        double low = this->get_parameter("low_soc_threshold").as_double();
        double target = this->get_parameter("charge_target_soc").as_double();
        if(!m_is_low && m_soc <= low)
        {
            m_is_low = true;
            RCLCPP_INFO(get_logger(), "battery low soc=%.2f", m_soc);
        }
        else if(m_is_low && m_soc >= target)
        {
            m_is_low = false;
            RCLCPP_INFO(get_logger(), "battery recovered soc=%.2f", m_soc);
        }

        BatteryState msg;
        msg.stamp = now;
        msg.battery = static_cast<float>(m_soc);
        msg.is_low = m_is_low;
        m_power_level_pub->publish(msg);
    }

private:
    rclcpp::Subscription<PowerMode>::SharedPtr m_power_mode_sub;
    rclcpp::Publisher<BatteryState>::SharedPtr m_power_level_pub;
    rclcpp::TimerBase::SharedPtr m_tick_timer;

    int32_t m_mode{PowerMode::MODE_IDLE};
    rclcpp::Time m_last_mode_stamp;
    rclcpp::Time m_last_tick;
    double m_soc{100.0};
    bool m_is_low{false};
};

}

int main(int argc, char** argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<amr_power_manager::AmrPowerManagerNode>());
    rclcpp::shutdown();
    return 0;
}
