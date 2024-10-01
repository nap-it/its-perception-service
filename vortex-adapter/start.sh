#!/bin/bash
export ROS_DOMAIN_ID=150
source /opt/ros/humble/setup.bash
source /vortex_adapter/vortex_perception_msgs/install/setup.bash

exec /vortex_adapter/build/main