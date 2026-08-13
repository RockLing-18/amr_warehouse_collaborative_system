# RobotPose
# 位姿话题 robot_pose 的消息类型（默认 1Hz，仅位姿，不含业务字段）
# 每机器人 namespace 下 topic: robot_pose

# 当前位姿（TF: map -> base_footprint）
geometry_msgs/PoseStamped pose

# 位姿是否有效（false = 未定位 / TF 查询失败）
bool pose_valid
