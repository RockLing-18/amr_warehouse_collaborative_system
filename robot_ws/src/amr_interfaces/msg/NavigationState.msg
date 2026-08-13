# NavigationState
# 导航状态常量 + 状态字段（供 amr_navigation / amr_manager 复用）
# C++ 引用常量：amr_interfaces::msg::NavigationState::STATE_NAVIGATING

uint8 IDLE=0
uint8 ACCEPTING=1
uint8 NAVIGATING=2
uint8 CANCELING=3
uint8 SUCCEEDED=4
uint8 FAILED=5
uint8 CANCELED=6

uint8 navigation_state
