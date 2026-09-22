#include "message/message_codec.h"
#include "nlohmann/json.hpp"
#include "utils/ros_logger.h"

using json = nlohmann::json;

namespace amr_agent
{
bool MessageCodec::decodeBootstrapInfo(const std::string& msg, BootstrapInfo& info)
{
    try
    {
        auto root = json::parse(msg);
        if(root.value("code", -1) != 0)
        {
            LOG_ERROR("bootstrap response code error");
            return false;
        }

        auto data = root["data"];
        info.warehouse_id = data.value("warehouse_id", "");

        auto mqtt = data["mqtt"];
        info.mqtt.url = mqtt.value("url", "");
        info.mqtt.client_id = mqtt.value("client_id", "");
        info.mqtt.user = mqtt.value("username", "");
        info.mqtt.pwd = mqtt.value("password", "");
        info.mqtt.keepalive = mqtt.value("keepalive", 12);
        info.mqtt.reconnect_interval = mqtt.value("reconnect_interval", 5);
        info.mqtt.tls_enable = mqtt.value("tls_enable", false);
        if(info.mqtt.url.empty())
        {
            LOG_ERROR("mqtt url empty");
            return false;
        }

        LOG_INFO(
            "bootstrap success get robot:%s warehouse:%s mqtt:%s",
            info.robot_id.c_str(),
            info.warehouse_id.c_str(),
            info.mqtt.url.c_str());
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("parse bootstrap response failed:%s", e.what());
        return false;
    }

    return true;
}

bool MessageCodec::decodeEdgeSvrStatus(const std::string& msg, bool& online)
{
    try
    {
        auto root = json::parse(msg);
        std::string status = root.value("status", "offline");
        if(status == "online")
            online = true;
        else
            online = false;
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("parse edgeSvrStatus failed:%s", e.what());
        return false;
    }

    return true;
}

bool MessageCodec::decodeRobotRegisterResp(const std::string& msg, RobotRegisterResponse& resp)
{
    try
    {
        auto root = json::parse(msg);
        if(root.value("code", -1) != 0)
        {
            LOG_ERROR("RobotRegister response code error");
            return false;
        }

        resp.request_id = root.value("request_id", "");
        resp.map_version = root.value("map_version", "");
        resp.map_update = root.value("map_update", false);
        resp.map_download_url = root.value("map_download_url", "");
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("parse RobotRegister response failed:%s", e.what());
        return false;
    }

    return true;
}

// std::string MessageCodec::encodeRegisterResp(const RobotRegisterResponse& resp)
// {
//     json rsp;
//     rsp["code"] = resp.code;
//     rsp["message"] = resp.message;
//     rsp["robot_id"] = resp.robot_id;
//     rsp["request_id"] = resp.request_id;
//     rsp["map_update"] = resp.map_update;
//     rsp["map_version"] = resp.map_version;
//     rsp["map_download_url"] = resp.map_download_url;
//     return rsp.dump();
// }


}