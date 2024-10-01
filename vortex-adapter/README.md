# VORTEX ADAPTER

This adapter's purpose is to integrate CPMs and CAMs into Vortex WorldModel.

## Configuration
Change the configuration of the adapter in the *config.ini* file
`[vortex-adapter]`
- dds_domain_id = <obu_dds_id> (to receive CAMs/CPMs from Vanetza)
- debug_level = 1 (or 0)
- reference_latitude = 40.634646 (Vortex Worldmodel reference latitude)
- reference_longitude = -8.659986  (Vortex Worldmodel reference longitude)
- cam_topic = vanetza/out/cam_full  (Topic where CAM messages are received)
- objects_topic = objects_full  (Topic where CPM objects are received)
- publish_topic = /in/objects (Topic to pulish the `vortex_perception_msgs/msg/observer` message)

To change the *ROS_DOMAIN_ID* for the adapter to publish, edit *start.sh*  and change the line:

`export ROS_DOMAIN_ID=<id>`

## Deployment

The *docker-compose.yml* file already deploys the service with the correct networking options and the script to start the service with the correct  *ROS_DOMAIN_ID*.
