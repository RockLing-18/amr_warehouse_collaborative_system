#pragma once

#include <cstdint>
#include <string>

namespace amr_common
{

// AMR 内部统一状态枚举。
// 数值越小可用度越低：amr_agent 聚合各模块状态时取最小值。
enum class RobotState : uint8_t
{
    ERROR = 0,          // 故障，不可用
    INITIALIZING = 1,   // 初始化中，不可接任务
    WORKING = 2,        // 正在执行任务
    IDLE = 3,           // 就绪空闲
};

inline const char* robotStateToString(RobotState state)
{
    switch(state)
    {
    case RobotState::ERROR: return "ERROR";
    case RobotState::INITIALIZING: return "INITIALIZING";
    case RobotState::WORKING: return "WORKING";
    case RobotState::IDLE: return "IDLE";
    default: return "INITIALIZING";
    }
}

inline RobotState robotStateFromString(const std::string& str)
{
    if(str == "ERROR") return RobotState::ERROR;
    if(str == "INITIALIZING") return RobotState::INITIALIZING;
    if(str == "WORKING") return RobotState::WORKING;
    if(str == "IDLE") return RobotState::IDLE;
    return RobotState::INITIALIZING;
}

}
