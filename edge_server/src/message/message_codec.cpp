#include "message/message_codec.h"
#include "nlohmann/json.hpp"
#include "utils/LogDefine.h"

using json = nlohmann::json;

namespace edge_server
{
bool MessageCodec::decodeRegisterReq(const std::string& payload, RobotRegisterRequest& req)
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

bool MessageCodec::decodeRobotRunningStatus(const std::string& payload, RobotRunningStatus& status)
{
    try
    {
        auto root = json::parse(payload);
        if(!root.contains("robot_id"))
        {
            LOG_ERROR("missing robot_id");
            return false;
        }

        if(!root.contains("timestamp"))
        {
            LOG_ERROR("missing timestamp");
            return false;
        }

        if(!root.contains("state"))
        {
            LOG_ERROR("missing state");
            return false;
        }

        if(!root.contains("pose"))
        {
            LOG_ERROR("missing pose");
            return false;
        }

        status.robot_id = root["robot_id"].get<std::string>();
        status.timestamp = root["timestamp"].get<uint64_t>();
        status.state = RobotStateFromString(root["state"].get<std::string>());
        status.online = true;
        status.battery = root.value("battery", 0.0);
        status.pose.x = root["pose"]["x"].get<double>();
        status.pose.y = root["pose"]["y"].get<double>();
        status.pose.yaw = root["pose"]["yaw"].get<double>();
        status.task_id = root.value("task_id", "");
        return true;
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("handleStatus exception:{}", e.what());
    }

    return false;
}

bool MessageCodec::decodeRobotWillPush(const std::string& payload, RobotRegWillPush& push)
{
    try
    {
        auto root = json::parse(payload);
        if(!root.contains("robot_id"))
        {
            LOG_ERROR("missing robot_id");
            return false;
        }

       push.robot_id = root.value("robot_id", "");
       return true;
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("handleWill exception:{}", e.what());
    }

    return false;
}


}