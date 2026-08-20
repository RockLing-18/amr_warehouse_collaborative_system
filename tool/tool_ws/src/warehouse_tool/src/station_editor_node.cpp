#include <atomic>
#include <iostream>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include "warehouse_tool/data_define.h"
#include "warehouse_tool/clicked_point_receiver.h"
#include "warehouse_tool/yaml_zone_manager.h"
#include "warehouse_tool/yaml_station_manager.h"
#include "warehouse_tool/zone_marker_publisher.h"
#include "warehouse_tool/station_marker_publisher.h"

class StationEditorNode : public rclcpp::Node
{
public:
    StationEditorNode() : Node("station_editor")
    {
        this->declare_parameter<std::string>("map_dir","");
    }

    ~StationEditorNode()
    {
        if(m_zone_marker_pub)
            m_zone_marker_pub->clear();
        
        m_running = false;
        if(m_terminal_thread.joinable())
        {
            m_terminal_thread.join();
        }
    }

    void init()
    {
        this->get_parameter("map_dir", m_map_dir);
        if(m_map_dir.empty())
        {
            RCLCPP_ERROR( get_logger(),"map_dir parameter is empty, please input param!");
            return;
        }

        std::string zone_file = m_map_dir + "/zones.yaml";
        if(!m_zone_manager.load(zone_file))
            RCLCPP_ERROR(get_logger(), "load zone file failed:%s", zone_file.c_str());
        else
            RCLCPP_INFO(get_logger(),"load zone file:%s",zone_file.c_str());

        std::string station_file = m_map_dir + "/stations.yaml";
        if(!m_station_manager.load(station_file))
            RCLCPP_ERROR(get_logger(), "load station file failed:%s", station_file.c_str());
        else
            RCLCPP_INFO(get_logger(),"load station file:%s", station_file.c_str());

        m_receiver = std::make_shared<ClickedPointReceiver>(
            shared_from_this(),
            std::bind(&StationEditorNode::onPointClicked, this, std::placeholders::_1)
        );

        m_zone_marker_pub =std::make_shared<ZoneMarkerPublisher>(shared_from_this());
        m_zone_marker_pub->publishZones(m_zone_manager.getZones());

        m_station_point_marker_pub =std::make_shared<StationMarkerPublisher>(shared_from_this());
        m_station_point_marker_pub->publishStations(m_station_manager.getStations());

        m_terminal_thread = std::thread(&StationEditorNode::terminalLoop, this);
    }

private:
    void startCollect()
    {
        if(m_collecting)
        {
            std::cout <<"already collecting\n";
            return;
        }

        m_preview_station = inputStationInfo();
        m_preview_station.locations.clear();
        m_receiver->start();
        m_collecting = true;
        std::cout <<"start click points in RViz\n";
    }

    void finishCollect()
    {
        if(!m_collecting)
            return;
        
        m_receiver->stop();
        m_collecting = false;

        if(m_preview_station.locations.empty())
        {
            std::cout << "locations is empty, do not save station\n";
            return;
        }

        m_station_point_marker_pub->clearPreview(m_preview_station.locations.size());
        if(m_station_manager.addStation(m_preview_station))
        {
            if(m_station_manager.save())
            {
                std::cout << "save station success\n";
                m_station_point_marker_pub->publishStation(m_preview_station);
            }
            else
                std::cout << "save station failed\n";
        }
        else
        {
            std::cout << "station id already exists\n";
        }

        return;
    }

    void terminalLoop()
    {
        while(m_running)
        {
            std::cout
            << "\ncommand:"
            << "\n start : begin collect"
            << "\n finish: stop collect"
            << "\n q     : quit"
            << "\n> ";
            std::string cmd;
            std::getline(std::cin, cmd);

            if(cmd == "start")
            {
                startCollect();
            }
            else if(cmd == "finish")
            {
                finishCollect();
            }
            else if(cmd == "q")
            {
                m_running=false;
                rclcpp::shutdown();
                break;
            }
        }
    }

    void onPointClicked(const geometry_msgs::msg::PointStamped& point)
    {
        if(point.header.frame_id != "map")
        {
            RCLCPP_WARN(get_logger(), "ignore click, frame not map, got:%s", point.header.frame_id.c_str());
            return;
        }

        m_receiver->stop();

        RCLCPP_INFO(get_logger(), "collect location x=%f y=%f", point.point.x, point.point.y);

        // m_preview_location = inputLocationInfo();
        m_preview_location.pose.x = point.point.x;
        m_preview_location.pose.y =  point.point.y;
        m_preview_location.pose.yaw = 0; //此处该赋什么值
        m_preview_location.type = "work";
        m_preview_location.id = m_preview_station.id + "_location_" + std::to_string(m_preview_station.locations.size() + 1);
        m_preview_station.locations.push_back(m_preview_location);
        m_station_point_marker_pub->publishPreview(m_preview_station);
        m_receiver->start();
    }

    Station inputStationInfo()
    {
        Station station;
        std::cout << "\n===== create station =====\n";
        std::cout << "station id:";
        std::getline(std::cin, station.id);

        std::cout << "station name:";
        std::getline(std::cin, station.name);

        std::cout << "station type:";
        std::getline(std::cin, station.type);

        station.zone_id = selectZone();
        return station;
    }
    
    Location inputLocationInfo()
    {
        Location location;
        // std::cout << "\nlocation id:";
        // std::getline(std::cin, location.id);
        std::cout << "location type:";
        std::getline( std::cin, location.type);
        return location;
    }

    std::string selectZone()
    {
        const auto& zones = m_zone_manager.getZones();
        if(zones.empty())
        {
            std::cout <<"no zone available\n";
            return "default_zone";
        }

        std::cout <<"\n===== select zone =====\n";
        for(size_t i = 0; i < zones.size(); ++i)
        {
            std::cout
            << i + 1
            << ". "
            << zones[i].info.id
            << "  "
            << zones[i].info.name
            << "\n";
        }

        int index = -1;
        std::cout << "select:";
        while(!(std::cin >> index))
        {
            std::cin.clear();
            std::cin.ignore( std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout<<"input number:";
        }

        if(index <= 0 || index > (int)zones.size())
        {
            std::cout << "invalid zone\n";
            return "default_zone";
        }
        return zones[index-1].info.id;
    }

private:
    std::shared_ptr<ClickedPointReceiver> m_receiver;
    std::shared_ptr<ZoneMarkerPublisher> m_zone_marker_pub;
    std::shared_ptr<StationMarkerPublisher> m_station_point_marker_pub;
    Station m_preview_station;
    Location m_preview_location;
    std::thread m_terminal_thread;
    std::atomic<bool> m_running{true};
    std::atomic<bool> m_collecting{false};
    YamlZoneManager m_zone_manager;
    YamlStationManager m_station_manager;
    std::string m_map_dir;
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    {
        auto node = std::make_shared<StationEditorNode>();
        node->init();
        rclcpp::spin(node);
    }
    
    rclcpp::shutdown();
    return 0;
}