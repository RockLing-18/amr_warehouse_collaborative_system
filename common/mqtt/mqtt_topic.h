#pragma once
#include "spdlog/fmt/fmt.h"
#include <string>

namespace mqtt_topic
{

// -----------------  Cloud / Edge / AMR 相关 MQTT topic  ---------------------//

constexpr const char EDGE_SERVER_STATUS[] = "edge_server/status";

constexpr const char ROBOT_STATUS[] = "amr/{}/status";  // 需要使用robot_id, 通配符+
constexpr const char ROBOT_WILL[] = "amr/{}/will";  // 需要使用robot_id, 通配符+

// AMR 向 edge server 注册相关
constexpr const char ROBOT_REGISTER_REQ[] = "amr/register/request";
constexpr const char ROBOT_REGISTER_RSP_PREFIX[] = "amr/register/response/";

constexpr const char MAP_REQUEST[] = "warehouse/map/request";

// Cloud -> Edge 任务下发/注入
// warehouse_id 会替换 {}
constexpr const char WAREHOUSE_TASK[] = "warehouse/{}/task";

// Edge -> Cloud 任务状态上报
// warehouse_id 会替换 {}
constexpr const char WAREHOUSE_TASK_STATUS[] = "warehouse/{}/task/status";

// Edge -> AMR 任务下发/取消
// robot_id 会替换 {}
constexpr const char AMR_TASK[] = "amr/{}/task";

// Edge -> AMR 任务共享状态
// warehouse_id、task_id 会替换 {}
constexpr const char WAREHOUSE_TASK_STATE[] = "warehouse/{}/task/{}/state";

// AMR -> Edge 搬运货物申请
// robot_id 会替换 {}
constexpr const char AMR_TASK_GOODS_REQUEST[] = "amr/{}/task/goods/request";

// Edge -> AMR 搬运货物申请响应
// robot_id 会替换 {}
constexpr const char AMR_TASK_GOODS_RESPONSE[] = "amr/{}/task/goods/response";

// AMR -> Edge 任务状态、完成、放弃/中断上报
// robot_id 会替换 {}
constexpr const char AMR_TASK_STATUS[] = "amr/{}/task/status";

constexpr const char TRAFFIC_RIGHTS_REQ[] = "amr/traffic_rights/request";
constexpr const char TRAFFIC_RIGHTS_RSP_PREFIX[] = "amr/traffic_rights/response/";


inline std::string makeRobotStatusTopic(const std::string& robot_id)
{
    return fmt::format(ROBOT_STATUS, robot_id);
}

inline std::string makeWarehouseTaskTopic(const std::string& warehouse_id)
{
    return fmt::format(WAREHOUSE_TASK, warehouse_id);
}

inline std::string makeWarehouseTaskStatusTopic(const std::string& warehouse_id)
{
    return fmt::format(WAREHOUSE_TASK_STATUS, warehouse_id);
}

inline std::string makeAmrTaskTopic(const std::string& robot_id)
{
    return fmt::format(AMR_TASK, robot_id);
}

inline std::string makeWarehouseTaskStateTopic(
    const std::string& warehouse_id,
    const std::string& task_id)
{
    return fmt::format(WAREHOUSE_TASK_STATE, warehouse_id, task_id);
}

inline std::string makeAmrTaskGoodsRequestTopic(const std::string& robot_id)
{
    return fmt::format(AMR_TASK_GOODS_REQUEST, robot_id);
}

inline std::string makeAmrTaskGoodsResponseTopic(const std::string& robot_id)
{
    return fmt::format(AMR_TASK_GOODS_RESPONSE, robot_id);
}

inline std::string makeAmrTaskStatusTopic(const std::string& robot_id)
{
    return fmt::format(AMR_TASK_STATUS, robot_id);
}

}
