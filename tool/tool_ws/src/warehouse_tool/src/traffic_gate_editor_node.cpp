#include <atomic>
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
#include "warehouse_tool/station_marker_publisher.h"

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
        m_station_marker_pub = std::make_shared<StationMarkerPublisher>(shared_from_this());
        publishAllGates();

        m_terminal_thread = std::thread(&TrafficGateEditorNode::terminalLoop, this);
    }

private:
    enum class Phase
    {
        Idle,
        Zone,
        L1,
        L2
    };

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

    Station toStation(const TrafficGate& gate) const
    {
        Station station;
        station.id = gate.resource_id;
        station.name = gate.resource_id;
        station.zone_id = "traffic_gate";

        int location_id = 1;
        auto addLocation = [&](const Pose2D& pose, const std::string& type)
        {
            Location location;
            location.id = gate.resource_id + "_wait_" + std::to_string(location_id++);
            location.type = type;
            location.marker_id = gate.marker_id * 100 + location_id;
            location.pose = pose;
            station.locations.push_back(location);
        };

        for(const auto& pose : gate.l1_wait_poses)
            addLocation(pose, "L1_WAIT");
        for(const auto& pose : gate.l2_wait_poses)
            addLocation(pose, "L2_WAIT");
        return station;
    }

    void publishAllGates()
    {
        const auto& gates = m_gate_manager.getGates();

        std::vector<Zone> zones;
        zones.reserve(gates.size());
        for(const auto& gate : gates)
            zones.push_back(toZone(gate));
        m_zone_marker_pub->publishZones(zones);

        std::vector<Station> stations;
        stations.reserve(gates.size());
        for(const auto& gate : gates)
            stations.push_back(toStation(gate));
        m_station_marker_pub->publishStations(stations);
    }

    void publishDraftPreview()
    {
        Zone zone;
        zone.info.id = "preview";
        zone.info.name = "drawing";
        zone.polygon = m_zone_points;
        m_zone_marker_pub->publishPreview(zone);

        Station station;
        station.id = "preview_wait";
        station.name = "drawing_wait";
        int marker_id = 1000;
        auto addLocation = [&](const Pose2D& pose, const std::string& type)
        {
            Location location;
            location.id = "preview_wait";
            location.type = type;
            location.marker_id = marker_id++;
            location.pose = pose;
            station.locations.push_back(location);
        };
        for(const auto& pose : m_l1_points)
            addLocation(pose, "L1_WAIT");
        for(const auto& pose : m_l2_points)
            addLocation(pose, "L2_WAIT");
        m_station_marker_pub->publishPreview(station);
    }

    void startCollection(Phase phase)
    {
        if(m_collecting)
        {
            std::cout << "already collecting\n" << std::flush;
            return;
        }

        m_phase = phase;
        m_receiver->start();
        m_collecting = true;
        if(phase == Phase::Zone)
            std::cout << "click 4 polygon points in RViz, then type finish\n" << std::flush;
        else
            std::cout << "click wait points in RViz, then type finish\n" << std::flush;
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
        if(m_pending_pose)
            processPendingWaitPose();

        stopCollection();

        if(m_phase == Phase::Zone)
        {
            if(m_zone_points.size() != 4)
            {
                std::cout << "narrow zone must have exactly 4 polygon points, current="
                          << m_zone_points.size() << "\n" << std::flush;
                m_phase = Phase::Idle;
                return;
            }
            std::cout << "zone ok. next: wait1 / wait2 / save\n" << std::flush;
        }
        else if(m_phase == Phase::L1)
        {
            if(m_l1_points.empty())
            {
                std::cout << "l1 wait poses is empty\n" << std::flush;
            }
            else
            {
                std::cout << "l1 wait poses collected: " << m_l1_points.size() << "\n" << std::flush;
            }
        }
        else if(m_phase == Phase::L2)
        {
            if(m_l2_points.empty())
            {
                std::cout << "l2 wait poses is empty\n" << std::flush;
            }
            else
            {
                std::cout << "l2 wait poses collected: " << m_l2_points.size() << "\n" << std::flush;
            }
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
        if(m_l1_points.empty() || m_l2_points.empty())
        {
            std::cout << "cannot save: l1 and l2 must each have at least 1 wait pose\n" << std::flush;
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
        std::cout << "gate created. use zone / wait1 / wait2 / save\n" << std::flush;
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

    void processPendingWaitPose()
    {
        if(!m_pending_pose)
            return;

        Pose2D pose;
        pose.x = m_pending_x;
        pose.y = m_pending_y;

        std::cout << "yaw for wait point (" << pose.x << ", " << pose.y << "): " << std::flush;
        std::string yaw_line;
        std::getline(std::cin, yaw_line);
        try
        {
            pose.yaw = std::stod(yaw_line);
        }
        catch(...)
        {
            pose.yaw = 0.0;
        }

        if(m_phase == Phase::L1)
            m_l1_points.push_back(pose);
        else if(m_phase == Phase::L2)
            m_l2_points.push_back(pose);

        m_pending_pose = false;
        publishDraftPreview();

        if(m_collecting)
            m_receiver->start();
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
            return;
        }

        if(m_phase == Phase::L1 || m_phase == Phase::L2)
        {
            m_receiver->stop();
            m_pending_x = point.point.x;
            m_pending_y = point.point.y;
            m_pending_pose = true;
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
                << "\n wait1 : collect l1 wait poses"
                << "\n wait2 : collect l2 wait poses"
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
            if(m_pending_pose)
            {
                processPendingWaitPose();
                menu_shown = false;
            }

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
                else if(cmd == "wait1")
                    startCollection(Phase::L1);
                else if(cmd == "wait2")
                    startCollection(Phase::L2);
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
    std::shared_ptr<StationMarkerPublisher> m_station_marker_pub;
    YamlTrafficGateManager m_gate_manager;
    std::string m_map_dir;

    std::thread m_terminal_thread;
    std::atomic<bool> m_running{true};
    std::atomic<bool> m_collecting{false};
    std::atomic<bool> m_pending_pose{false};
    bool m_editing{false};

    Phase m_phase{Phase::Idle};
    std::string m_resource_id;
    std::vector<std::pair<double,double>> m_zone_points;
    std::vector<Pose2D> m_l1_points;
    std::vector<Pose2D> m_l2_points;
    double m_pending_x{0.0};
    double m_pending_y{0.0};
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
