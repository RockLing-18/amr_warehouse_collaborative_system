#pragma once
#include <string>
#include "types.h"

namespace edge_server
{
class MessageCodec
{
public:
    static bool decodeRegisterReq(const std::string& payload, RobotRegisterRequest& req);
    static std::string encodeRegisterResp(const RobotRegisterResponse& resp);
    static bool decodeRobotRunningStatus(const std::string& payload, RobotRunningStatus& status);
    static bool decodeRobotWillPush(const std::string& payload, RobotRegWillPush& push);
};
}