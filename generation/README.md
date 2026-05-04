# Generation Service

The Generation service is the core component of the CPS. It periodically queries the object cache for fresh detections, constructs an ETSI TS 103 324 compliant CPM, and publishes it via DDS for transmission by the V2X stack.

### Internal Components

| Class | Responsibility |
|---|---|
| `Aggregator` | Subscribes to `generation/objects` and `generation/sensors` via DDS/Zenoh. Maintains a live per-object cache and applies ETSI priority rules to determine which objects are fresh enough for the next CPM. |
| `Builder` | Stateless CPM JSON constructor. Converts fresh objects and sensor metadata into the ETSI CPM structure. Handles coordinate transformation (absolute lat/lon → relative x/y), ETSI unit scaling, and timestamp conversion (UNIX → ETSI 2004 epoch). |
| `Locator` | Supplies the current station position. In `static` mode it uses fixed coordinates from config. In `mqtt`/`dds` mode it tracks the station position from incoming CAM or VAM messages. |

### Priority Modes

The Aggregator supports two priority modes, configured via `aggregator.priority`:

- **`etsi`** *(default)* — Uses the ETSI TS 103 324 priority thresholds: position change (≥4 m), speed change (≥0.5 m/s), heading change (≥4°), time since last inclusion (≥1000 ms). An object is included in the next CPM if any threshold is exceeded.

- **`predictor`** — A movement-prediction-based priority that weights objects by how much their estimated future position deviates from their last-reported state. Useful for fast-moving objects where ETSI thresholds may be too conservative.

Setting `aggregator.ignore_rules = true` bypasses both modes and includes all tracked objects in every CPM.

## `config.ini` Configuration

```ini
[general]
debug = false

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

| Key | Default | Description |
|---|---|---|
| `general.debug` | `false` | Enable debug-level logging |
| `locator.station_type` | `15` | ETSI station type: `5` for OBU, `15` for RSU |
| `locator.station_latitude` | `0.0` | Station latitude (degrees) — used when `location_provider = static` |
| `locator.station_longitude` | `0.0` | Station longitude (degrees) — used when `location_provider = static` |
| `locator.location_provider` | `static` | Position source: `static`, `mqtt`, or `dds` |
| `locator.mqtt_host` | `127.0.0.1` | MQTT broker address — used when `location_provider = mqtt` |
| `locator.mqtt_port` | `1883` | MQTT broker port |
| `locator.mqtt_topic` | `""` | CAM/VAM topic — supported: `vanetza/in/cam`, `vanetza/in/cam_full`, `vanetza/own/cam`, `vanetza/in/vam` |
| `locator.domain_id` | `0` | DDS domain ID — **change this** to match the location data DDS domain when `location_provider = dds` |
| `locator.dds_topic` | `""` | CAM/VAM DDS topic — same supported values as `mqtt_topic` |
| `aggregator.domain_id` | `0` | DDS domain ID — **change this** to match the Sensor Adapters domain |
| `aggregator.max_object_age` | `2` | Maximum age (seconds) before an object is evicted from the cache |
| `aggregator.clean_interval` | `1` | How often (seconds) the cache cleanup runs |
| `aggregator.ignore_rules` | `false` | If `true`, all cached objects are included in every CPM regardless of freshness |
| `aggregator.performance_logs` | `false` | Write CSV performance logs to `/logs/aggregator.csv` |
| `aggregator.priority` | `etsi` | Priority mode: `etsi` or `predictor` |
| `aggregator.zenoh_endpoint` | `""` | Zenoh locator for peer/router discovery (leave empty to disable Zenoh) |
| `aggregator.add_pending_objects` | `false` | Include objects that did not meet freshness thresholds in the previous cycle |
| `generation.interval` | `100` | CPM generation interval (milliseconds) |
| `generation.domain_id` | `0` | DDS domain ID — **change this** to match the Vanetza/V2X stack domain |
| `generation.dds_topic` | `vanetza/in/cpm` | DDS topic to publish CPMs — also supports `cps-v2/in/cpm` for loopback testing |
| `generation.performance_logs` | `false` | Write CSV performance logs to `/logs/generation.csv` |
| `generation.max_objects` | `-1` | Maximum objects per CPM (`-1` = no limit) |
| `generation.mqtt_debug` | `false` | Mirror each CPM to local MQTT topic `mqtt/in/cpm` for debugging |
