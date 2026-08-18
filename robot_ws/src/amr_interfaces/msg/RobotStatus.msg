# RobotStatus
# amr_manager 聚合发布的机器人状态（每机器人 namespace 下 topic: robot_status）

string robot_id

int32 STATE_IDLE=0
int32 STATE_EXECUTING=1
int32 STATE_CHARGING=2
int32 STATE_ERROR=3
int32 STATE_SAFE_MODE=4
int32 state

float32 battery                 # 电量百分比 0~100
float64 position_x
float64 position_y
float64 yaw
bool navigating                 # 是否正在导航
string current_task_id
builtin_interfaces/Time stamp
