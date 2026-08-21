#include "warehouse_tool/clicked_point_receiver.h"
#include "warehouse_tool/data_define.h"
#include <atomic>
#include <iostream>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include "warehouse_tool/yaml_zone_manager.h"
#include "warehouse_tool/zone_marker_publisher.h"

class ZoneEditorNode : public rclcpp::Node
{
public:
    ZoneEditorNode() : Node("zone_editor")
    {
        this->declare_parameter<std::string>("map_dir","");
    }

    ~ZoneEditorNode()
    {
        if(m_marker_pub)
            m_marker_pub->clear();
        
        m_running=false;
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

        m_receiver = std::make_shared<ClickedPointReceiver>(
            shared_from_this(),
            std::bind(&ZoneEditorNode::onPointClicked, this, std::placeholders::_1)
        );

        m_marker_pub =std::make_shared<ZoneMarkerPublisher>(shared_from_this());
        m_marker_pub->publishZones(m_zone_manager.getZones());

        m_terminal_thread = std::thread(&ZoneEditorNode::terminalLoop, this);
    }

private:
    void startCollect()
    {
        if(m_collecting)
        {
            std::cout <<"already collecting\n";
            return;
        }

        m_vPoint.clear();
        m_receiver->start();
        m_collecting = true;
        std::cout <<"start click points in RViz\n" << std::flush;
    }

    void finishCollect()
    {
        if(!m_collecting)
            return;
        
        if(m_vPoint.size() < 3)
        {
            std::cout<<"polygon need >=3 points\n" << std::flush;
            m_receiver->stop();
            m_marker_pub->clearPreview();
            m_collecting = false;
            return;
        }

        m_receiver->stop();
        m_collecting = false;
        std::cout << "collect finished\n" << std::flush;
        std::cout <<"total points:" << m_vPoint.size() << std::endl << std::flush;
        for(auto& p : m_vPoint)
        {
            std::cout <<"(" << p.first << "," << p.second << ")\n" << std::flush;
        }

        Zone zone = inputZoneInfo();
        zone.polygon = m_vPoint;
        if(m_zone_manager.addZone(zone))
        {
            if(m_zone_manager.save())
            {
                std::cout << "save zone success\n" << std::flush;
                m_marker_pub->clearPreview();
                m_marker_pub->publishZone(zone);
            }
            else
                std::cout << "save zone failed\n" << std::flush;
        }
        else
        {
            std::cout << "zone id already exists\n" << std::flush;
        }
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
            << "\n> "
            << std::flush;
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
            RCLCPP_WARN(get_logger(),"ignore click, frame not map, got:%s", point.header.frame_id.c_str());
            return;
        }

        RCLCPP_INFO(get_logger(),"click x=%f y=%f", point.point.x, point.point.y);
        m_vPoint.emplace_back(point.point.x, point.point.y);

        Zone preview;
        preview.info.id = "preview";
        preview.info.name = "drawing";
        preview.polygon = m_vPoint;
        m_marker_pub->publishPreview(preview);
    }

    Zone inputZoneInfo()
    {
        Zone zone;

        std::cout << "zone id: ";
        std::getline(std::cin, zone.info.id);
        std::cout << "zone name: ";
        std::getline(std::cin, zone.info.name);
        std::cout << "zone type: ";
        std::getline(std::cin, zone.info.type);
        return zone;
    }
    
private:
    std::shared_ptr<ClickedPointReceiver> m_receiver;
    std::shared_ptr<ZoneMarkerPublisher> m_marker_pub;
    std::vector<std::pair<double,double>> m_vPoint;
    std::thread m_terminal_thread;
    std::atomic<bool> m_running{true};
    std::atomic<bool> m_collecting{false};
    YamlZoneManager m_zone_manager;
    std::string m_map_dir;
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    {
        auto node = std::make_shared<ZoneEditorNode>();
        node->init();
        rclcpp::spin(node);
    }
    
    rclcpp::shutdown();
    return 0;
}