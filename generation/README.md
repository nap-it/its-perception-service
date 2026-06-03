# Generation Service

The Generation service is the core component of the ITS Perception Service. It receives object and sensor information from the Adapters services through DDS or Zenoh, maintains an internal cache of detected objects, and periodically evaluates which objects should be included in a Collective Perception Message (CPM) according to ETSI TS 103 324 rules.
At each generation cycle, the service constructs an ETSI-compliant CPM containing the selected objects and publishes it through DDS for transmission by the V2X stack or consumption by other services.


## Internal Components

| Class | Responsibility |
|---|---|
| `Aggregator` | Subscribes to `generation/objects` and `generation/sensors` via DDS/Zenoh. Maintains a per-object cache and applies ETSI rules to determine which objects should be send in the next CPM. |
| `Builder` | Builds ETSI-compliant CPM JSON messages. Converts the object to send and sensor metadata into the ETSI CPM structure. Handles coordinate transformation (absolute lat/lon → relative x/y), ETSI unit scaling, and timestamp conversion (UNIX → ETSI 2004 epoch). |
| `Locator` | Provides the current station position. In `static` mode it uses fixed coordinates from config. In `mqtt`/`dds` mode it tracks the station position from incoming CAM or VAM messages. |

## Communication Interfaces
The Generation service exchanges information with other ITS Perception Service components and external systems through the following communication interfaces:

| Interface | Direction | Technologies |
|---|---|---|
| Object input | Sensor Adapters → Generation | DDS, Zenoh |
| Sensor metadata input | Sensor Adapters → Generation | DDS, Zenoh |
| Station location input | CAM/VAM → Locator | DDS, MQTT |
| CPM output | Generation → V2X stack / Processing | DDS |
| Debug CPM output | Generation → MQTT | MQTT |

## Generation Cycle

At each generation interval, the service performs the following steps:

1. Retrieves eligible objects from the Aggregator
2. Retrieves the current station position from the Locator
3. Builds the CPM structure using the Builder
4. Publishes the generated CPM through DDS

## Object Inclusion and Priority

The Aggregator performs two different calculations when new object information arrives: object inclusion and object priority.

### Object Inclusion

Object inclusion determines whether a tracked object is eligible to be included in the next CPM.

By default, an object is included if at least one of the ETSI TS 103 324 inclusion conditions is met since the object was last included in a CPM:

- position change ≥ 4 m
- speed change ≥ 0.5 m/s
- heading change ≥ 4°
- time since last inclusion ≥ 1000 ms

If `aggregator.ignore_rules = true`, these checks are disabled and all tracked objects are considered eligible for inclusion.

### Object Priority

Object priority determines the order of eligible objects inside the CPM. If a maximum number of objects per CPM is configured, only the objects with the highest priority values are included.

The selected priority mode is configured in the configuration file.

The Aggregator supports two priority modes:

- **`etsi`** *(default)* — Computes the object priority using ETSI TS 103 324 priority components. The final priority is based on the sum of:
  - object perception quality
  - object position change
  - object speed change
  - object orientation change
  - time since the object was last included in a CPM

- **`predictor`** — Computes the object priority based on prediction error. This mode compares the predicted object state with the updated object state and assigns higher priority to objects whose movement differs more from the prediction.


## `config.ini` Configuration

The Generation service is configured through a `config.ini` file mounted into the container.

```ini
[general]
log_level = info
prometheus = true
prometheus_port = 9102

[locator]
station_type = 15
station_latitude = 40.630280
station_longitude = -8.654225
location_provider = static ; static, mqtt or dds
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

| Key | Default | Description | Range |
|---|---|---|---|
| `general.log_level` | `info` | Logging verbosity | `trace`, `debug`, `info`, `warn`, `error`, `critical`, `off` |
| `general.prometheus` | `false` | Enable Prometheus metrics endpoint | `true/false` |
| `general.prometheus_port` | `9102` | Port for the Prometheus metrics HTTP endpoint | `1024`–`65535` |
| `locator.station_type` | `15` | ETSI station type: `5` for OBU, `15` for RSU | `5/15` |
| `locator.station_latitude` | `0.0` | Station latitude (degrees) — used when `location_provider = static` | `-90.0` to `90.0` |
| `locator.station_longitude` | `0.0` | Station longitude (degrees) — used when `location_provider = static` | `-180.0` to `180.0` |
| `locator.location_provider` | `static` | Position source: `static`, `mqtt`, or `dds` | - |
| `locator.mqtt_host` | `127.0.0.1` | MQTT broker address — used when `location_provider = mqtt` | - |
| `locator.mqtt_port` | `1883` | MQTT broker port | - |
| `locator.mqtt_topic` | `""` | CAM/VAM topic — supported: `vanetza/in/cam`, `vanetza/in/cam_full`, `vanetza/own/cam`, `vanetza/in/vam` | - |
| `locator.domain_id` | `0` | DDS domain ID — Must match the location data DDS domain when `location_provider = dds` | `0` to `230` |
| `locator.dds_topic` | `""` | CAM/VAM DDS topic — same supported values as `mqtt_topic` | - |
| `aggregator.domain_id` | `0` | DDS domain ID — Must match the Sensor Adapters domain | `0` to `230` |
| `aggregator.max_object_age` | `2` | Maximum age (seconds) before an object is evicted from the cache | `>0` |
| `aggregator.clean_interval` | `1` | How often (seconds) the cache cleanup runs | `>0` |
| `aggregator.ignore_rules` | `false` | If `true`, all cached objects are eligible for inclusion in every CPM | `true/false` |
| `aggregator.performance_logs` | `false` | Write CSV performance logs to `/logs/aggregator.csv` | `true/false` |
| `aggregator.priority` | `etsi` | Priority mode: `etsi` or `predictor` | - |
| `aggregator.zenoh_endpoint` | `""` | Zenoh locator for peer/router discovery (leave empty to disable Zenoh) | - |
| `aggregator.add_pending_objects` | `false` | Include objects that were not eligible for inclusion in the previous cycle if there is remaining space available in the CPM. Typically used when a maximum number of objects per CPM is configured | `true/false` |
| `generation.interval` | `100` | CPM generation interval (milliseconds) | `>0` |
| `generation.domain_id` | `0` | DDS domain ID — Must match the Vanetza/V2X stack domain | `0` to `230` |
| `generation.dds_topic` | `vanetza/in/cpm` | DDS topic to publish CPMs — also supports `cps-v2/in/cpm` for loopback testing | - |
| `generation.performance_logs` | `false` | Write CSV performance logs to `/logs/generation.csv` | `true/false` |
| `generation.max_objects` | `-1` | Maximum objects per CPM (`-1` = no limit) | `>0` or `-1` |
| `generation.mqtt_debug` | `false` | Publishes a copy of each CPM to local MQTT topic `mqtt/in/cpm` for debugging | `true/false` |


