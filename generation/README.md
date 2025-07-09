
## [`config.ini`](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/blob/main/generation/config.ini) configuration

The contents of the `config.ini`are as follows:

```ini

[general]
debug = false

[locator]
station_type = 15
station_latitude = 40.630280
station_longitude = -8.654225
location_provider = static ; mqtt or dds
mqtt_host =
mqtt_port =
mqtt_topic =
domain_id = 0
dds_topic =

[aggregator]
domain_id = 0
max_object_age = 2
clean_interval = 1
ignore_rules = false
performance_logs = false
priority = etsi  ; etsi (default) or predictor
zenoh_endpoint = 127.0.0.1
add_pending_objects = false

[generation]
interval = 100
domain_id = 0
dds_topic = vanetza/in/cpm
performance_logs = false
max_objects = -1
mqtt_debug = false
```

| .ini file key                 | Default                       | Notes |
| -------------                 |-------------                      |-------------|
| general.debug                         | false     | Debug flag to extend the logs |
| locator.station_type            | 15             | Station type 5 for OBU or 15 for RSU |
| locator.station_latitude            | 0.0             | Latitude of the station if RSU (static)|
| locator.station_longitude            | 0.0             | Longitude of the station if RSU (static)|
| locator.location_provider            | static             | Location provider, either `static`, `mqtt` or `dds` |
| locator.domain_id            | 0             | **Change this** depending on the Location Data domain id when location_provider is `dds` |
| locator.mqtt_host            | 127.0.0.1             | MQTT host to publish station data when location_provider is `mqtt` |
| locator.mqtt_port            | 1883             | MQTT port to publish station data when location_provider is `mqtt` |
| locator.mqtt_topic            | ""             | Supported topics: `vanetza/in/cam`, `vanetza/in/cam_full`, `vanetza/own/cam`, `vanetza/in/vam` |
| locator.dds_topic            | ""             | Supported topics: `vanetza/in/cam`, `vanetza/in/cam_full`, `vanetza/own/cam`, `vanetza/in/vam` |
| aggregator.domain_id            | 0             | **Change this** depending on the *Sensor Adapters* domain id |
| aggregator.max_object_age            | 2             | Maximum age of objects in cache in seconds |
| aggregator.clean_interval            | 1             | Interval to clean old objects in the cache in seconds |
| aggregator.ignore_rules            | false             | Ignore the rules for the CPM dissemination |
| aggregator.performance_logs            | false             | Enable CSV performance logs under the `logs`folder |
| aggregator.priority            | etsi             | Priority of the objects, either `etsi` or `predictor` |
| aggregator.zenoh_endpoint            | ""             | Zenoh endpoint to find peers/routers |
| aggregator.add_pending_objects            | false             | Add pending objects to the next CPM|
| generation.interval            | 100             | Interval to generate a new CPM in milliseconds |
| generation.domain_id            | 0             | **Change this** depending on the *Vanetza* domain id |
| generation.dds_topic            | ""             | Topic to publish the CPMs using DDS. Supported topics: `vanetza/in/cpm`, `cps-v2/in/cpm` |
| generation.performance_logs            | false             | Enable CSV performance logs under the `logs`folder |
| generation.max_objects            | -1             | Maximum number of objects to be included in the CPM, `-1` for no limit |
| generation.mqtt_debug            | false             | Enable publishing of CPM in local MQTT topic `mqtt/in/cpm` for debugging purposes |
