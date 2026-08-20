#include "amr_tools/clicked_point_receiver.h"
#include "amr_tools/data_define.h"
#include <atomic>
#include <iostream>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include "amr_tools/yaml_wall_manager.h"
#include "amr_tools/wall_marker_publisher.h"

class WallEditorNode : public rclcpp::Node
{
public:
    WallEditorNode() : Node("wall_editor")
    {
        this->declare_parameter<std::string>("map_dir","");
    }

    ~WallEditorNode()
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

        std::string wall_file = m_map_dir + "/walls.yaml";
        if(!m_wall_manager.load(wall_file))
            RCLCPP_ERROR(get_logger(), "load wall file failed:%s", wall_file.c_str());
        else
            RCLCPP_INFO(get_logger(),"load wall file:%s",wall_file.c_str());

        m_receiver = std::make_shared<ClickedPointReceiver>(
            shared_from_this(),
            std::bind(&WallEditorNode::onPointClicked, this, std::placeholders::_1)
        );

        m_marker_pub =std::make_shared<WallMarkerPublisher>(shared_from_this());
        m_marker_pub->publishWalls(m_wall_manager.getWalls());

        m_terminal_thread = std::thread(&WallEditorNode::terminalLoop, this);
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
        std::cout <<"start click points in RViz\n";
    }

    void finishCollect()
    {
        if(!m_collecting)
            return;
        
        if(m_vPoint.size() != 2)
        {
            std::cout<<"polygon need 2 points\n";
            m_receiver->stop();
            m_marker_pub->clearPreview();
            m_collecting = false;
            return;
        }

        m_receiver->stop();
        m_collecting = false;
        std::cout << "collect finished\n";
        std::cout <<"total points:" << m_vPoint.size() << std::endl;
        for(auto& p : m_vPoint)
        {
            std::cout <<"(" << p.first << "," << p.second << ")\n";
        }

        Wall wall = inputWallInfo();
        wall.line = m_vPoint;
        if(m_wall_manager.addWall(wall))
        {
            if(m_wall_manager.save())
            {
                std::cout << "save wall success\n";
                m_marker_pub->clearPreview();
                m_marker_pub->publishWall(wall);
            }
            else
                std::cout << "save wall failed\n";
        }
        else
        {
            std::cout << "wall id already exists\n";
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
            RCLCPP_WARN(get_logger(),"ignore click, frame not map, got:%s", point.header.frame_id.c_str());
            return;
        }

        RCLCPP_INFO(get_logger(),"click x=%f y=%f", point.point.x, point.point.y);
        m_vPoint.emplace_back(point.point.x, point.point.y);

        Wall preview;
        preview.id = "preview";
        preview.line = m_vPoint;
        m_marker_pub->publishPreview(preview);
    }

    Wall inputWallInfo()
    {
        Wall wall;

        std::cout << "wall id: ";
        std::getline(std::cin, wall.id);
        return wall;
    }
    
private:
    std::shared_ptr<ClickedPointReceiver> m_receiver;
    std::shared_ptr<WallMarkerPublisher> m_marker_pub;
    std::vector<std::pair<double,double>> m_vPoint;
    std::thread m_terminal_thread;
    std::atomic<bool> m_running{true};
    std::atomic<bool> m_collecting{false};
    YamlWallManager m_wall_manager;
    std::string m_map_dir;
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    {
        auto node = std::make_shared<WallEditorNode>();
        node->init();
        rclcpp::spin(node);
    }
    
    rclcpp::shutdown();
    return 0;
}