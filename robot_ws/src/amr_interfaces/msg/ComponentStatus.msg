# 模块统一状态（amr_navigation / amr_behavior_manager 上报，amr_agent 聚合）
# 数值越小可用度越低，amr_agent 取各模块最小值

int32 STATE_ERROR=0
int32 STATE_INITIALIZING=1
int32 STATE_CHARGING=2
int32 STATE_PAUSED=3
int32 STATE_WORKING=4
int32 STATE_IDLE=5

int32 state
builtin_interfaces/Time stamp
