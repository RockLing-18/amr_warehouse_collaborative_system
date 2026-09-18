#include "bootstrap/bootstrap.h"
#include "http/http_client.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include "yaml-cpp/yaml.h"
#include "message/message_codec.h"

namespace amr_agent
{

bool Bootstrap::run(const rclcpp::Logger& logger, const std::string& cfgPath, BootstrapInfo& info)
{
    // 1. 加载本地配置
    if(!loadLocalConfig(logger, cfgPath, info))
    {
        RCLCPP_ERROR(logger, "load local config failed");
        return false;
    }

    // 2. 请求edge server bootstrap
    if(!requestBootstrap(logger, info))
    {
        RCLCPP_ERROR(logger, "request bootstrap failed");
        return false;
    }

    return true;
}

bool Bootstrap::loadLocalConfig(const rclcpp::Logger& logger, const std::string& cfgPath, BootstrapInfo& info)
{
    try
    {
        YAML::Node config = YAML::LoadFile(cfgPath);
        if(config["robot"] && config["robot"]["id"])
        {
            info.robot_id = config["robot"]["id"].as<std::string>();
            m_robotId = info.robot_id;
        }

        if(config["edge_server"])
        {
            if(config["edge_server"]["host"])
            {
                m_edgeHost = config["edge_server"]["host"].as<std::string>();
            }

            if(config["edge_server"]["port"])
            {
                m_edgePort =  config["edge_server"]["port"].as<int>();
            }
        }

        if(config["map"])
        {
            if(config["map"]["path"])
            {
                info.map.path = config["map"]["path"].as<std::string>();
                
                if(!loadMapMetadata(logger, info))
                    info.map.version = "";
            }
        }

        if(m_robotId.empty())
        {
            RCLCPP_ERROR(logger, "robot id empty");
            return false;
        }

        if(m_edgeHost.empty())
        {
            RCLCPP_ERROR(logger, "edge host empty");
            return false;
        }

        RCLCPP_INFO(
            logger,
            "load config success robot_id:%s edge:%s:%d",
            m_robotId.c_str(),
            m_edgeHost.c_str(),
            m_edgePort);
    }
    catch(const std::exception& e)
    {
        RCLCPP_ERROR(logger, "load config exception:%s", e.what());
        return false;
    }

    return true;
}

bool Bootstrap::loadMapMetadata(const rclcpp::Logger& logger, BootstrapInfo& info)
{
    try
    {
        std::string sFilePath = info.map.path + "/metadata.yaml";
        YAML::Node config = YAML::LoadFile(sFilePath);
        if(config["version"])
        {
            info.map.version = config["version"].as<std::string>();
        }

        if(config["files"])
        {
            if(config["files"]["map"])
            {
                info.map.mapName = config["files"]["map"].as<std::string>();
            }

            if(config["files"]["image"])
            {
                info.map.imageName =  config["files"]["image"].as<std::string>();
            }

            if(config["files"]["zone"])
            {
                info.map.zoneName = config["files"]["zone"].as<std::string>();
            }

            if(config["files"]["station"])
            {
                info.map.stationName =  config["files"]["station"].as<std::string>();
            }
        }

        if(config["map"])
        {
            if(config["map"]["path"])
            {
                info.map.path = config["map"]["path"].as<std::string>();

                loadMapMetadata(logger, info);
            }
        }

        if(m_robotId.empty())
        {
            RCLCPP_ERROR(logger, "robot id empty");
            return false;
        }

        if(m_edgeHost.empty())
        {
            RCLCPP_ERROR(logger, "edge host empty");
            return false;
        }

        RCLCPP_INFO(
            logger,
            "load config success robot_id:%s edge:%s:%d",
            m_robotId.c_str(),
            m_edgeHost.c_str(),
            m_edgePort);
    }
    catch(const std::exception& e)
    {
        RCLCPP_ERROR(logger, "load config exception:%s", e.what());
        return false;
    }

    return true;
}

bool Bootstrap::requestBootstrap(const rclcpp::Logger& logger, BootstrapInfo& info)
{
    HttpClient client(m_edgeHost, m_edgePort);

    std::string path = "/api/amr/bootstrap?robot_id="  + m_robotId;
    auto response =  client.get(path);
    if(!response.succeed)
    {
        RCLCPP_ERROR(logger, "bootstrap http failed:%s", response.errMsg.c_str());
        return false;
    }

    return MessageCodec::decodeBootstrapInfo(logger, response.body, info);
}

}