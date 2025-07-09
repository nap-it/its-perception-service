
## [`config.ini`](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/blob/main/generation/config.ini) configuration

The contents of the `config.ini`are as follows:

```ini

[processing]
debug=true
domain_id=0
station_type=15                             
station_id=0                               
performance_logs=false

dds_output_topic=objects                               
dds_output_full_topic=objects_full                     

local_mqtt_enable_publisher=true
local_mqtt_host=127.0.0.1
local_mqtt_port=1883
local_mqtt_output_topic=objects                        
local_mqtt_output_full_topic=objects_full              

remote_mqtt_enable_publisher=false
remote_mqtt_host=atcll-services.nap.av.it.pt
remote_mqtt_port=1884
remote_mqtt_username=atcll-services
remote_mqtt_password=
remote_mqtt_output_topic=p1/objects
remote_mqtt_output_full_topic=p1/objects_full

zenoh_endpoint = 127.0.0.1
zenoh_output_topic = objects
zenoh_output_full_topic = objects_full

[locator]
station_id=1 
location_provider = static ; mqtt or dds
mqtt_host =
mqtt_port =
mqtt_topic =
domain_id = 0
dds_topic =
```

| .ini file key                 | Default                       | Notes |
| -------------                 |-------------                      |-------------|
| processing.debug                         | true     | Debug flag to extend the logs |
| processing.domain_id            | 0             | **Change this** depending on the *Vanetza* or *Generation* domain id |
| processing.station_type            | 15             | Only needed when station is processing own messages |
| processing.station_id            | 1             | Only needed when station is processing own messages |
| processing.performance_logs            | false             | Enable CSV performance logs under the `logs`folder |
| processing.dds_output_topic            | objects             | Topic to publish the processed objects using DDS
| processing.dds_output_full_topic            | objects_full             | Topic to publish the processed objects using DDS in full format |
| processing.local_mqtt_enable_publisher            | true             | Enable publishing of processed objects in local MQTT broker |
| processing.local_mqtt_host            | 127.0.0.1             | Local MQTT host to publish processed objects |
| processing.local_mqtt_port            | 1883             | Local MQTT port to publish processed objects |
| processing.local_mqtt_output_topic            | objects             | Local MQTT topic to publish processed objects |
| processing.local_mqtt_output_full_topic            | objects_full             | Local MQTT topic to publish processed objects in full format |
| processing.remote_mqtt_enable_publisher            | false             | Enable publishing of processed objects in remote MQTT broker |
| processing.remote_mqtt_host            | 127.0.0.1             | Remote MQTT host to publish processed objects |
| processing.remote_mqtt_port            | 1883             | Remote MQTT port to publish processed objects |
| processing.remote_mqtt_username            | ""             | Remote MQTT username to publish processed objects |
| processing.remote_mqtt_password            | ""             | Remote MQTT password to publish processed objects |
| processing.remote_mqtt_output_topic            | objects             | Remote MQTT topic to publish processed objects |
| processing.remote_mqtt_output_full_topic            | objects_full             | Remote MQTT topic to publish processed objects in full format |
| processing.zenoh_endpoint            | 127.0.0.1             | Zenoh endpoint to find peers/routers |
| processing.zenoh_output_topic            | objects             | Zenoh topic to publish processed objects |
| processing.zenoh_output_full_topic            | objects_full             | Zenoh topic to publish processed objects in full format |
| locator.station_id            | 1             | Only needed when station is processing own messages |
| locator.location_provider            | static             | Location provider for receive CPMs, either `static`, `mqtt` or `dds` |
| locator.mqtt_host            | 127.0.0.1             | MQTT host to publish station data when location_provider is `mqtt` |
| locator.mqtt_port            | 1883             | MQTT port to publish station data when location_provider is `mqtt` |
| locator.mqtt_topic            | ""             | Supported topics: `vanetza/in/cam`, `vanetza/own/cam`, `vanetza/out/cam`, `vanetza/in/cam_full`, `vanetza/in/vam`, `vanetza/out/cam_full` |
| locator.domain_id            | 0             | **Change this** depending on the who is providing station data domain id when location_provider is `dds` |
| locator.dds_topic            | ""             | Supported topics: `vanetza/in/cam`, `vanetza/own/cam`, `vanetza/out/cam`, `vanetza/in/cam_full`, `vanetza/in/vam`, `vanetza/out/cam_full` |


