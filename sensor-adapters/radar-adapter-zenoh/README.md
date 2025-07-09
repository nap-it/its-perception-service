
## [`config.ini`](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/blob/main/sensor-adapters/radar-adapter/config.ini) configuration

The contents of the `config.ini`are as follows:

```ini
[radar-adapter]
domain_id=0
debug=false
shared_memory=true
zenoh_endpoint = 127.0.0.1
mqtt_host=127.0.0.1
mqtt_port=1883
mqtt_topic=jetson/radar-plus
mqtt_client_id=radar-adapter

```

| .ini file key                 | Default                       | Notes |
| -------------                 |-------------                      |-------------|
| domain_id                 | 0             | **Change this** depending on the *Generation* domain id |
| debug                         | false     | Debug flag to extend the logs|
| shared_memory                 | true     | Use shared memory for Zenoh communication (default is false)|
| zenoh_endpoint                 | 127.0.0.1     | Optional Zenoh endpoint to connect to peers/routers|
| mqtt_host                         | 127.0.0.1      | Broker where radar objects are published|
| mqtt_host                         | 1883      | Port for the broker|
| mqtt_topic                         | jetson/radar-plus      | Topic where radar objects are published|
| mqtt_client_id                        | radar-adapter      | Client ID for the MQTT connection|