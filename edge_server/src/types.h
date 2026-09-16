#pragma once
#include <string>

namespace edge_server
{

struct EdgeServer
{
    std::string id;
};

struct Warehouse
{
    std::string id;
};

struct MqttMessage
{
    std::string topic;
    std::string payload;
};

struct WebSocket
{
    std::string host;
    int port;
    std::string protocol;
};

struct Robot
{
    int list_period_ms;
};

struct LogCfg
{
    std::string level;
};

struct SqliteCfg
{
    std::string path;
};


struct MqttCfg
{
    std::string url;
    std::string client_id;
    std::string user;
    std::string pwd;
    std::string ca_file;
    int keepalive;
    int reconnect_interval;
    bool tls_enable{false};
    bool will_msg_enable{false};
    MqttMessage will;
};

struct HttpCfg
{
    std::string host;
    int port;
    std::string ca_file;
    bool tls_enable{false};
};

struct Config
{
    EdgeServer edge_server;
    Warehouse warehouse;
    WebSocket websocket;
    Robot robot;
    LogCfg log;
    MqttCfg edge_amr_mqtt;
    HttpCfg http;
    SqliteCfg sqlite;
};

enum class RobotState
{
    OFFLINE = 0,
    INITIALIZING,  // 正在初始化,不可接任务
    IDLE,          // 空闲，可接任务
    WORKING,       // 执行任务中
    CHARGING,      // 充电
    PAUSED,        // 暂停
    ERROR,         // 故障
};

// 机器人位姿
struct Pose2D
{
    double x;
    double y;
    double yaw;
};

struct RobotBaseInfo
{
    std::string robot_id;
    std::string simulation_instance_id;
    std::string warehouse_id;
    std::string map_version;
    uint64_t register_timestamp;  // 注册的时间戳, 暂时以其作为instance_i
};

struct RobotRunningStatus
{
    std::string robot_id;
    RobotState state;
    bool online;
    uint64_t timestamp;
    double battery;
    Pose2D pose;
    std::string task_id;
};

struct RobotInstanceInfo
{
    std::string robot_id;
    std::string simulation_instance_id;
    Pose2D pose;
};


struct MapPackage
{
    // 地图ID
    int64_t id = 0;

    // 所属仓库
    std::string warehouse_id;

    // 地图版本
    // v1.0
    std::string version;

    // zip文件名称
    // warehouse_01_package_v1.0.zip
    std::string package_name;

    // edge server存储路径
    std::string package_path;

    // 文件大小
    uint64_t package_size = 0;

    // edge server上传时间
    std::string upload_time;

    // 是否上传后立即启用
    bool activate{false};
};

struct MapUploadRequest
{
    std::string warehouse_id;
    std::string version;

    /*
     * 文件名称
     */
    std::string package_name;
    int64_t package_size = 0;

    bool activate{false};

    /*
     * 上传文件临时路径
     *
     * 例如:
     *
     * /tmp/upload/map.zip
     */
    std::string upload_file_path;
};

struct MapUpdateInfo
{
    bool need_update=false;
    std::string version;
    std::string download_url;
};






struct RobotRegisterRequest
{
    std::string robot_id;
    std::string simulation_instance_id;
    std::string warehouse_id;
    std::string map_version;
    std::string request_id;
    uint64_t register_timestamp;  // 注册的时间戳, 暂时以其作为instance_id
};


struct RobotRegisterResponse
{
    int code = -1;
    std::string message;
    std::string robot_id;
    std::string request_id;
    std::string map_version;
    std::string map_download_url;
    bool map_update = false;
};

struct RobotRegWillPush
{
    std::string robot_id;
};



// 字符串 -> 枚举（MQTT收到消息时调用）
inline RobotState RobotStateFromString(const std::string& str)
{
    if(str == "OFFLINE") return RobotState::OFFLINE;
    if(str == "INITIALIZING") return RobotState::INITIALIZING;
    if(str == "IDLE") return RobotState::IDLE;
    if(str == "WORKING") return RobotState::WORKING;
    if(str == "CHARGING") return RobotState::CHARGING;
    if(str == "PAUSED") return RobotState::PAUSED;
    if(str == "ERROR") return RobotState::ERROR;
    return RobotState::OFFLINE; // 非法字符串兜底
}

// 枚举 -> 字符串（上报MQTT、打印日志）
inline std::string RobotStateToString(RobotState s)
{
    switch(s)
    {
        case RobotState::OFFLINE: return "UNKOFFLINENOWN";
        case RobotState::INITIALIZING: return "INITIALIZING";
        case RobotState::IDLE: return "IDLE";
        case RobotState::WORKING: return "WORKING";
        case RobotState::CHARGING: return "CHARGING";
        case RobotState::PAUSED: return "PAUSED";
        case RobotState::ERROR: return "ERROR";
        default: return "OFFLINE";
    }
}

}