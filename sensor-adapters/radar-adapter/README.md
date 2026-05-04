# Radar Adapter

The Radar Adapter bridges a radar detection pipeline into the CPS Generation service. It subscribes to one or more MQTT topics carrying object detections from a radar, normalises the data into the CPS object format, and publishes it to the DDS topic `generation/objects`.

### Data Flow

```
Radar pipeline → MQTT "<mqtt_topic>" → Radar Adapter → DDS "generation/objects"
```

### Expected Input (MQTT)

Each MQTT message must be a JSON object containing an `objects` array. Each element must include at minimum: `objectID`, `sensorID`, `timestamp`, `latitude`, `longitude`, `heading`, `speed`, `confidence`, and `classification`. The `size_x` field (object width) is supported and included when present. Optional fields not provided by radar pipelines (`altitude`, `size_y`, `size_z`, covariances) default to `0.0`.

See the root [README.md](../../README.md#object-data----input-topic-generationobjects) for the full field reference.

## `config.ini` Configuration

```ini
[radar-adapter]
domain_id = 0
debug = false
mqtt_host = 127.0.0.1
mqtt_port = 1883
mqtt_topic = jetson/radar-plus
mqtt_client_id = radar-adapter
```

| Key | Default | Description |
|---|---|---|
| `domain_id` | `0` | DDS domain ID — **change this** to match `aggregator.domain_id` in the Generation config |
| `debug` | `false` | Enable debug-level logging |
| `mqtt_host` | `127.0.0.1` | MQTT broker address where radar detections are published |
| `mqtt_port` | `1883` | MQTT broker port |
| `mqtt_topic` | `jetson/radar-plus` | MQTT topic(s) to subscribe to. Supports comma-separated values for multiple radars |
| `mqtt_client_id` | `radar-adapter` | MQTT client identifier |
