#pragma once

namespace amr_common::topic
{

inline constexpr char MAP_INFO[] = "map_info";
inline constexpr char ROBOT_STATUS[] = "robot_status";
inline constexpr char TASK_STATUS[] = "task_status";
inline constexpr char ROBOT_POSE[] = "robot_pose";

}

namespace amr_common::service
{

inline constexpr char GET_ROBOT_POSE[] = "get_robot_pose";

}

namespace amr_common::action
{

inline constexpr char AMR_NAVIGATE_TO_POSE[] = "amr_navigate_to_pose";

}
