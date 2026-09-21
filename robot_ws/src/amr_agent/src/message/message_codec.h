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
    //static std::string encodeRegisterResp(const RobotRegisterResponse& resp);
};
}