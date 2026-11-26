#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <poll.h>
#include <unistd.h>

#include "warehouse_tool/clicked_point_receiver.h"
#include "warehouse_tool/data_define.h"
#include "warehouse_tool/yaml_traffic_gate_manager.h"
#include "warehouse_tool/zone_marker_publisher.h"
#include "warehouse_tool/traffic_gate_marker_publisher.h"

class TrafficGateEditorNode : public rclcpp::Node
{
public:
    TrafficGateEditorNode() : Node("traffic_gate_editor")
    {
        this->declare_parameter<std::string>("map_dir", "");
    }

    ~TrafficGateEditorNode()
    {
        if(m_zone_marker_pub)
            m_zone_marker_pub->clear();
        if(m_wait_marker_pub)
            m_wait_marker_pub->clear();

        m_running = false;
        if(m_terminal_thread.joinable())
            m_terminal_thread.join();
    }

    void init()
    {
        this->get_parameter("map_dir", m_map_dir);
        if(m_map_dir.empty())
        {
            RCLCPP_ERROR(get_logger(), "map_dir parameter is empty, please input param!");
            return;
        }

        std::string traffic_gate_file = m_map_dir + "/traffic_gates.yaml";
        if(!m_gate_manager.load(traffic_gate_file))
            RCLCPP_ERROR(get_logger(), "load traffic_gates file failed:%s", traffic_gate_file.c_str());
        else
            RCLCPP_INFO(get_logger(), "load traffic_gates file:%s", traffic_gate_file.c_str());

        m_receiver = std::make_shared<ClickedPointReceiver>(
            shared_from_this(),
            std::bind(&TrafficGateEditorNode::onPointClicked, this, std::placeholders::_1));

        m_zone_marker_pub = std::make_shared<ZoneMarkerPublisher>(shared_from_this());
        m_wait_marker_pub = std::make_shared<TrafficGateMarkerPublisher>(shared_from_this());
        publishAllGates();

        m_terminal_thread = std::thread(&TrafficGateEditorNode::terminalLoop, this);
    }

private:
    enum class Phase
    {
        Idle,
        Zone,
        Waits
    };

    static double distance(
        const std::pair<double,double>& a,
        const std::pair<double,double>& b)
    {
        double dx = a.first - b.first;
        double dy = a.second - b.second;
        return std::sqrt(dx * dx + dy * dy);
    }

    std::pair<double,double> longAxis() const
    {
        if(m_zone_points.size() != 4)
            return {0.0, 0.0};
        double dx = m_zone_points[2].first - m_zone_points[1].first;
        double dy = m_zone_points[2].second - m_zone_points[1].second;
        double len = std::sqrt(dx * dx + dy * dy);
        if(len < 1e-6)
            return {0.0, 0.0};
        return {dx / len, dy / len};
    }

    std::pair<double,double> centroid() const
    {
        if(m_zone_points.empty())
            return {0.0, 0.0};
        double x = 0.0;
        double y = 0.0;
        for(const auto& p : m_zone_points)
        {
            x += p.first;
            y += p.second;
        }
        return {x / m_zone_points.size(), y / m_zone_points.size()};
    }

    bool validateZoneOrder() const
    {
        if(m_zone_points.size() != 4)
            return false;

        double l12 = distance(m_zone_points[0], m_zone_points[1]);
        double l23 = distance(m_zone_points[1], m_zone_points[2]);
        double l34 = distance(m_zone_points[2], m_zone_points[3]);
        double l41 = distance(m_zone_points[3], m_zone_points[0]);

        if(l23 < l12 * 1.5 || l23 < l34 * 1.5 || l41 < l12 * 1.5)
        {
            std::cout
                << "point order invalid. Please click as P1->P2 short edge, "
                << "P2->P3 long axis, P3->P4 short edge\n"
                << std::flush;
            return false;
        }
        return true;
    }

    Zone toZone(const TrafficGate& gate) const
    {
        Zone zone;
        zone.info.id = gate.resource_id;
        zone.info.name = gate.resource_id;
        zone.info.type = "TRAFFIC_GATE";
        zone.polygon = gate.polygon;
        zone.marker_id = gate.marker_id;
        return zone;
    }

    void publishAllGates()
    {
        const auto& gates = m_gate_manager.getGates();

        std::vector<Zone> zones;
        zones.reserve(gates.size());
        for(const auto& gate : gates)
            zones.push_back(toZone(gate));
        m_zone_marker_pub->publishZones(zones);
        m_wait_marker_pub->publishAll(gates);
    }

    void publishDraftPreview()
    {
        Zone zone;
        zone.info.id = "preview";
        zone.info.name = "drawing";
        zone.polygon = m_zone_points;
        m_zone_marker_pub->publishPreview(zone);

        TrafficGate gate;
        gate.resource_id = m_resource_id.empty() ? "preview" : m_resource_id;
        gate.polygon = m_zone_points;
        gate.l1_wait_poses = m_l1_points;
        gate.l2_wait_poses = m_l2_points;
        m_wait_marker_pub->publishPreview(gate);
    }

    void startCollection(Phase phase)
    {
        if(m_collecting)
        {
            std::cout << "already collecting\n" << std::flush;
            return;
        }

        if(phase == Phase::Waits && m_zone_points.size() != 4)
        {
            std::cout << "please collect 4 zone points first (command: zone)\n" << std::flush;
            return;
        }

        m_phase = phase;
        m_receiver->start();
        m_collecting = true;
        if(phase == Phase::Zone)
            std::cout << "click P1->P2 short edge, then P2->P3 long axis, then P3->P4 short edge; type finish\n" << std::flush;
        else
            std::cout << "click wait points; each point is auto-assigned to l1/l2; type finish\n" << std::flush;
    }

    void stopCollection()
    {
        if(!m_collecting)
            return;
        m_receiver->stop();
        m_collecting = false;
    }

    void finishCollection()
    {
        stopCollection();

        if(m_phase == Phase::Zone)
        {
            if(m_zone_points.size() != 4)
            {
                std::cout << "narrow zone must have exactly 4 points, current="
                          << m_zone_points.size() << "\n" << std::flush;
            }
            else if(!validateZoneOrder())
            {
                std::cout << "zone rejected, please re-collect\n" << std::flush;
            }
            else
            {
                std::cout << "zone ok. next: waits / save\n" << std::flush;
            }
        }
        else if(m_phase == Phase::Waits)
        {
            std::cout << "wait points collected: l1=" << m_l1_points.size()
                      << " l2=" << m_l2_points.size() << "\n" << std::flush;
            if(m_l1_points.empty() || m_l2_points.empty())
                std::cout << "warning: each side should have at least 1 wait point\n" << std::flush;
        }

        m_phase = Phase::Idle;
        publishDraftPreview();
    }

    void saveGate()
    {
        if(m_zone_points.size() != 4)
        {
            std::cout << "cannot save: polygon must have exactly 4 points\n" << std::flush;
            return;
        }
        if(!validateZoneOrder())
            return;
        if(m_l1_points.empty() || m_l2_points.empty())
        {
            std::cout << "cannot save: l1 and l2 must each have at least 1 wait point\n" << std::flush;
            return;
        }

        TrafficGate gate;
        gate.resource_id = m_resource_id;
        gate.polygon = m_zone_points;
        gate.l1_wait_poses = m_l1_points;
        gate.l2_wait_poses = m_l2_points;

        if(!m_gate_manager.addGate(gate))
        {
            std::cout << "resource_id already exists\n" << std::flush;
            return;
        }

        if(!m_gate_manager.save())
        {
            std::cout << "save traffic_gates failed\n" << std::flush;
            return;
        }

        std::cout << "save traffic_gate success\n" << std::flush;
        publishAllGates();
        resetDraft();
    }

    void resetDraft()
    {
        m_resource_id.clear();
        m_zone_points.clear();
        m_l1_points.clear();
        m_l2_points.clear();
        m_phase = Phase::Idle;
        m_editing = false;
        stopCollection();
        m_zone_marker_pub->clearPreview();
        m_wait_marker_pub->clearPreview();
    }

    void createGate()
    {
        if(m_editing)
        {
            std::cout << "a gate is being edited, save or cancel first\n" << std::flush;
            return;
        }

        std::cout << "resource_id: " << std::flush;
        std::getline(std::cin, m_resource_id);
        if(m_resource_id.empty())
        {
            std::cout << "resource_id is empty\n" << std::flush;
            return;
        }

        m_zone_points.clear();
        m_l1_points.clear();
        m_l2_points.clear();
        m_phase = Phase::Idle;
        m_editing = true;
        std::cout << "gate created. use zone / waits / save\n" << std::flush;
    }

    void listGates()
    {
        const auto& gates = m_gate_manager.getGates();
        std::cout << "traffic_gates: " << gates.size() << "\n";
        for(const auto& gate : gates)
        {
            std::cout << "  " << gate.resource_id
                      << " polygon=" << gate.polygon.size()
                      << " l1=" << gate.l1_wait_poses.size()
                      << " l2=" << gate.l2_wait_poses.size() << "\n";
        }
    }

    void onPointClicked(const geometry_msgs::msg::PointStamped& point)
    {
        if(point.header.frame_id != "map")
        {
            RCLCPP_WARN(get_logger(), "ignore click, frame not map, got:%s", point.header.frame_id.c_str());
            return;
        }

        if(m_phase == Phase::Zone)
        {
            m_zone_points.emplace_back(point.point.x, point.point.y);
            publishDraftPreview();
            std::cout << "zone point " << m_zone_points.size()
                      << " (" << point.point.x << ", " << point.point.y << ")\n" << std::flush;
            return;
        }

        if(m_phase == Phase::Waits)
        {
            if(m_zone_points.size() != 4)
            {
                RCLCPP_WARN(get_logger(), "ignore wait point: zone points not ready");
                return;
            }

            auto axis = longAxis();
            auto center = centroid();
            double s = (point.point.x - center.first) * axis.first +
                       (point.point.y - center.second) * axis.second;

            Pose2D pose;
            pose.x = point.point.x;
            pose.y = point.point.y;

            if(s < 0.0)
            {
                pose.yaw = std::atan2(axis.second, axis.first);
                m_l1_points.push_back(pose);
                std::cout << "wait point -> l1, yaw=" << pose.yaw << "\n" << std::flush;
            }
            else
            {
                pose.yaw = std::atan2(-axis.second, -axis.first);
                m_l2_points.push_back(pose);
                std::cout << "wait point -> l2, yaw=" << pose.yaw << "\n" << std::flush;
            }

            publishDraftPreview();
        }
    }

    void printMenu(bool editing)
    {
        if(!editing)
        {
            std::cout
                << "\ncommand:"
                << "\n new : create traffic gate"
                << "\n list: list gates"
                << "\n q   : quit"
                << "\n> " << std::flush;
        }
        else
        {
            std::cout
                << "\ncommand:"
                << "\n zone  : collect 4 polygon points"
                << "\n waits : collect wait points (auto l1/l2)"
                << "\n finish: finish current collection"
                << "\n save  : save current gate"
                << "\n cancel: discard current gate"
                << "\n q     : quit"
                << "\n> " << std::flush;
        }
    }

    void terminalLoop()
    {
        bool menu_shown = false;
        while(m_running)
        {
            if(!menu_shown)
            {
                printMenu(m_editing);
                menu_shown = true;
            }

            struct pollfd pfd{};
            pfd.fd = STDIN_FILENO;
            pfd.events = POLLIN;
            int ret = poll(&pfd, 1, 100);
            if(ret <= 0)
                continue;

            std::string cmd;
            if(!std::getline(std::cin, cmd))
                continue;

            if(!m_editing)
            {
                if(cmd == "new")
                    createGate();
                else if(cmd == "list")
                    listGates();
                else if(cmd == "q")
                {
                    m_running = false;
                    rclcpp::shutdown();
                    break;
                }
            }
            else
            {
                if(cmd == "zone")
                    startCollection(Phase::Zone);
                else if(cmd == "waits")
                    startCollection(Phase::Waits);
                else if(cmd == "finish")
                    finishCollection();
                else if(cmd == "save")
                    saveGate();
                else if(cmd == "cancel")
                {
                    resetDraft();
                    std::cout << "gate discarded\n" << std::flush;
                }
                else if(cmd == "q")
                {
                    m_running = false;
                    rclcpp::shutdown();
                    break;
                }
            }

            menu_shown = false;
        }
    }

private:
    std::shared_ptr<ClickedPointReceiver> m_receiver;
    std::shared_ptr<ZoneMarkerPublisher> m_zone_marker_pub;
    std::shared_ptr<TrafficGateMarkerPublisher> m_wait_marker_pub;
    YamlTrafficGateManager m_gate_manager;
    std::string m_map_dir;

    std::thread m_terminal_thread;
    std::atomic<bool> m_running{true};
    std::atomic<bool> m_collecting{false};
    bool m_editing{false};

    Phase m_phase{Phase::Idle};
    std::string m_resource_id;
    std::vector<std::pair<double,double>> m_zone_points;
    std::vector<Pose2D> m_l1_points;
    std::vector<Pose2D> m_l2_points;
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    {
        auto node = std::make_shared<TrafficGateEditorNode>();
        node->init();
        rclcpp::spin(node);
    }
    rclcpp::shutdown();
    return 0;
}
