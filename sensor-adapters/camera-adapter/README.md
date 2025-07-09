
## [`config.ini`](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/blob/main/sensor-adapters/camera-adapter/config.ini) configuration

The contents of the `config.ini`are as follows:

```ini
[camera-adapter]
domain_id=0
debug=false
mqtt_host=127.0.0.1
mqtt_port=1883
mqtt_topic=jetson/camera/1/tracking/objects
mqtt_client_id=camera-adapter

```

| .ini file key                 | Default                       | Notes |
| -------------                 |-------------                      |-------------|
| domain_id                 | 0             | **Change this** depending on the *Generation* domain id |
| debug                         | false     | Debug flag to extend the logs|
| mqtt_host                         | 127.0.0.1      | Broker where camera objects are published|
| mqtt_host                         | 1883      | Port for the broker|
| mqtt_topic                         | jetson/camera/1/tracking/objects      | Topic where camera objects are published|
| mqtt_client_id                        | camera-adapter      | Client ID for the MQTT connection|