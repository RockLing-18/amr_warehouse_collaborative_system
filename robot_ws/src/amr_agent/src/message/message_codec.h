#pragma once
#include <string>
#include <rclcpp/rclcpp.hpp>
#include "types.h"

namespace amr_agent
{
class MessageCodec
{
public:
    static bool decodeBootstrapInfo(const std::string& msg, BootstrapInfo& info);
    static bool decodeEdgeSvrStatus(const std::string& msg, bool& online);
    static bool decodeRobotRegisterResp(const std::string& msg, RobotRegisterResponse& resp);
    //static std::string encodeRegisterResp(const RobotRegisterResponse& resp);
};
}