#pragma once
#include "spdlog/fmt/fmt.h"
#include <string>

namespace mqtt_topic
{

// -----------------  edge server 与 AMR 之间的topic  ---------------------//

constexpr char EDGE_SERVER_STATUS[] = "edge_server/status";

constexpr char ROBOT_STATUS[] = "amr/{}/status";  // 需要使用robot_id, 通配符+

// AMR 向 edge server 注册相关
constexpr char ROBOT_REGISTER_REQ[] = "amr/register/request";
constexpr char ROBOT_REGISTER_RSP_PREFIX[] = "amr/register/response/";

constexpr char MAP_REQUEST[] = "warehouse/map/request";

constexpr char TRAFFIC_RIGHTS_REQ[] = "amr/traffic_rights/request";
constexpr char TRAFFIC_RIGHTS_RSP_PREFIX[] = "amr/traffic_rights/response/";


inline std::string makeRobotStatusTopic(const std::string& robot_id)
{
    return fmt::format(ROBOT_STATUS, robot_id);
}

}
