#pragma once

#include <memory>
#include <string>
#include <rclcpp/rclcpp.hpp>
#include "amr_interfaces/msg/map_info.hpp"
#include "types.h"

namespace amr_agent
{

class MapManager
{
public:
    explicit MapManager(const rclcpp::Node::SharedPtr& node);

    bool init(const std::string& path);

    void onRegisterResponse(const RobotRegisterResponse& response);

    bool isReady() const;

    std::string getMapDir() const;
    std::string getMapYaml() const;

private:
    bool updateMap(const RobotRegisterResponse& response);

    bool downloadMap(const std::string& url, const std::string& packagePath);

    bool extractMap(const std::string& packagePath, const std::string& mapDir);

    bool activateMap(const std::string& mapDir);

    bool loadCurrentMap();
    void publishMapInfo();

private:
    rclcpp::Node::WeakPtr m_node;
    rclcpp::Publisher<amr_interfaces::msg::MapInfo>::SharedPtr m_mapInfoPublisher;
    std::string m_mapRootPath;

    std::string m_warehouseId;
    std::string m_mapVersion;
    std::string m_mapDir;
    std::string m_mapYaml;

    bool m_ready{false};
};

}