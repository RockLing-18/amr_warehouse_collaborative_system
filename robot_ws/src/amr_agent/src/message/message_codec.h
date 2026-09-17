#pragma once
#include <string>
#include "types.h"

namespace amr_agent
{
class MessageCodec
{
public:
    static bool decodeBootstrapInfo(const std::string& msg, BootstrapInfo& req);
    static std::string encodeRegisterResp(const RobotRegisterResponse& resp);
};
}