#include "message/message_codec.h"
#include "rclcpp/rclcpp.hpp"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace amr_agent
{
bool MessageCodec::decodeBootstrapInfo(const std::string& msg, BootstrapInfo& req)
{
    try
    {   
       auto root = json::parse(payload);
        if(!root.contains("robot_id"))
        {
            LOG_ERROR("register missing robot_id");
            return false;
        }

        if(!root.contains("timestamp"))
        {
            LOG_ERROR("register missing timestamp");
            return false;
        }

        if(!root.contains("warehouse_id"))
        {
            LOG_ERROR("register missing warehouse_id");
            return false;
        }

        if(!root.contains("warehouse_id"))
        {
            LOG_ERROR("register missing warehouse_id");
            return false;
        }

        if(!root.contains("map_version"))
        {
            LOG_ERROR("register missing map_version");
            return false;
        }

        req.robot_id = root.value("robot_id", "");
        req.register_timestamp = root.value("timestamp", 0);
        req.warehouse_id = root.value("warehouse_id", "");
        req.map_version = root.value("map_version", "");
        req.request_id = root.value("request_id", "");
        return true;
     }
    catch(const std::exception& e)
    {
        LOG_ERROR("robot register exception:{}", e.what());
    }

    return false;
}

std::string MessageCodec::encodeRegisterResp(const RobotRegisterResponse& resp)
{
    json rsp;
    rsp["code"] = resp.code;
    rsp["message"] = resp.message;
    rsp["robot_id"] = resp.robot_id;
    rsp["request_id"] = resp.request_id;
    rsp["map_update"] = resp.map_update;
    rsp["map_version"] = resp.map_version;
    rsp["map_download_url"] = resp.map_download_url;
    return rsp.dump();
}


}