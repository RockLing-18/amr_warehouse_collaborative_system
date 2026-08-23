#!/bin/bash

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ROS2 Humble
source /opt/ros/humble/setup.bash

# Robot workspace
source "$PROJECT_ROOT/robot_ws/install/setup.bash"

# Simulation workspace
source "$PROJECT_ROOT/simulation_ws/install/setup.bash"