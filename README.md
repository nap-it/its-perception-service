# Collective Perception Service v2

## Configuration
###### CPSv2 has a set of configurable attributes with the goal of allowing for fine-tuning its operation.

#### [Radar Adapter Config](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/blob/main/sensor-adapters/radar-adapter/config.ini)

| .ini file key                 | Description                       | Notes |
| -------------                 |-------------                      |-------------|
| mqtt_host                     | Local MQTT Broker IP              | Default is *mosquitto*  |
| mqtt_port                     | Local MQTT Broker Port            | Default is *1883*|
| mqtt_topic                    | MQTT Topic with Radar information | Default is *jetson/radar-plus*|
| mqtt_client_id                | MQTT Client ID                    | **Usually not** updated |
| clean_last_sent_interval      | Interval (ms) to call the function that cleans old objects| Internal use|
| max_object_age                | Maximum Age (ms) of each old object to be cleaned    | Internal use|
| **domain_id**                 | DDS Domain ID             | **Update** depending on the deployed station |
| debug                         | Debug flag to extend the logs     | Internal use|

#### [Camera Adapter Config](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/blob/main/sensor-adapters/camera-adapter/config.ini)

| .ini file key                 | Description                       | Notes |
| -------------                 |-------------                      |-------------|
| mqtt_host                     | Local MQTT Broker IP              | Default is *mosquitto* |
| mqtt_port                     | Local MQTT Broker Port            | Default is *1883* |
| mqtt_topic                    | MQTT Topic with Radar information | Default is *jetson/camera/tracking/objects*|
| mqtt_client_id                | MQTT Client ID                     | **Usually not** updated
| clean_last_sent_interval      | Interval (ms) to call the function that cleans old objects| Internal use|
| max_object_age                | Maximum Age (ms) of each old object to be cleaned    | Internal use|
| **domain_id**                 | DDS Domain ID             | **Update** depending on the deployed station |
| debug                         | Debug flag to extend the logs     | Internal use|

#### [Generation](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/blob/main/generation/config.ini)

| .ini file key                 | Description                       | Notes |
| -------------                 |-------------                      |-------------|
| debug                         | Debug flag to extend the logs     | Internal use |
| **domain_id**                 | DDS Domain ID             | **Update** depending on the deployed station |
| **station_type**              | Station type              | **Update** depending on the deployed station |
| **expected_responses**        | Number of active connected adapters | **Update** depending on the deployed environment |
| **latitude**                  | Station's latitude value  | **Update** used on stations that don't have CAMs available (e.g. RSU) |
| **longitude**                 | Station's longitude value | **Update** used on stations that don't have CAMs available (e.g. RSU) |
| subscribe_cam_topic           | DDS Topic to subscribe CAMs       | Default is *vanetza/in/cam_full*, usually for OBUs |
| publish_cpm_topic             | DDS Topic to publish CPMs         | Default is *cps-v2/in/cpm* to avoid using Vanetza in RSUs|
| request_deadline              | Request deadline (ms) to wait for adapter responses | Internal use |
| request_interval              | Request interval for new objects  | Internal use |
| max_interval                  | Maximum request interval          | Internal use |
| clean_object_interval         | Interval (ms) to clean old object ids | Internal use |
| mqtt_enable_publish           | Enable publishing CPMs via MQTT   | Disabled by default|
| mqtt_host                     | MQTT Broker IP                    | Default is *mosquitto* |
| mqtt_port                     | MQTT Broker IP                    | Default is *1883* |
| mqtt_publish_cpm_topic        | MQTT Topic to publish CPMs        | Default is *cps-v2/in/cpm*|

#### [Processing](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/blob/main/processing/config.ini)

| .ini file key                 | Description                       | Notes |
| -------------                 |-------------                      |-------------|
| debug                         | Debug flag to extend the logs     | Internal use |
| **domain_id**                 | DDS Domain ID                     | **Update** depending on the deployed station |
| subscribe_cpm_topic           | Number of active connected adapters | Update depending on the deployed environment (in RSUs should be the same as Generation) |
| dds_publish_objects_topic     | DDS Objects topic                         | Default is *objects* |
| dds_publish_objects_full_topic     | DDS Objects Full information topic   | Default is *objects_full* |
| **local_mqtt_enable_publisher**       | Enable publishing CPMs via MQTT   | **Enabled** by default for RSUs|
| local_mqtt_host                       | MQTT Broker IP                    | Default is *mosquitto* |
| local_mqtt_port                       | MQTT Broker IP                    | Default is *1883* |
| local_mqtt_publish_objects_topic      | MQTT Publish CPM topic            | Default is *objects*|
| local_mqtt_publish_objects_full_topic | MQTT Publish CPM topic            | Default is *objects_full*|
| **remote_mqtt_enable_publisher**           | Enable publishing CPMs via MQTT   | **Disabled** by default for RSUs|
| remote_mqtt_host                       | MQTT Broker IP                    | Can be used for ATCLL|
| remote_mqtt_port                       | MQTT Broker IP                    | Can be used for ATCLL|
| remote_mqtt_publish_objects_topic      | MQTT Publish CPM topic            | **Update** depending on the deployed station, default is *p0/objects*|
| remote_mqtt_publish_objects_full_topic | MQTT Publish CPM topic            | **Update** depending on the deployed station, default is *p0/objects_full*|


## Deployment
###### Instructions to deploy CPSv2 in different environments in provided [here](https://wiki.nap.av.it.pt/en/groups/nap/atcll/cps-deployment)

## Token
Private token: `glpat-BZYHmcoyr2u-Bsx1sFoZ`
