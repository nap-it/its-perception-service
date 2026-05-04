# Camera Adapter

The Camera Adapter bridges a camera detection pipeline into the CPS Generation service. It subscribes to one or more MQTT topics carrying object detections from a camera (e.g. a Jetson-based tracking pipeline), normalises the data into the CPS object format, and publishes it to the DDS topic `generation/objects`.

### Data Flow

```
Camera pipeline → MQTT "<mqtt_topic>" → Camera Adapter → DDS "generation/objects"
```

### Expected Input (MQTT)

Each MQTT message must be a JSON object containing an `objects` array. Each element must include at minimum: `objectID`, `sensorID`, `timestamp`, `latitude`, `longitude`, `heading`, `speed`, `confidence`, and `classification`. Optional fields (`altitude`, `size_*`, covariances) default to `0.0` when absent.

See the root [README.md](../../README.md#object-data----input-topic-generationobjects) for the full field reference.

## `config.ini` Configuration

```ini
[camera-adapter]
domain_id = 0
debug = false
mqtt_host = 127.0.0.1
mqtt_port = 1883
mqtt_topics = jetson/camera/1/tracking/objects,jetson/camera/2/tracking/objects
mqtt_client_id = camera-adapter
```

| Key | Default | Description |
|---|---|---|
| `domain_id` | `0` | DDS domain ID — **change this** to match `aggregator.domain_id` in the Generation config |
| `debug` | `false` | Enable debug-level logging |
| `mqtt_host` | `127.0.0.1` | MQTT broker address where camera detections are published |
| `mqtt_port` | `1883` | MQTT broker port |
| `mqtt_topics` | `jetson/camera/1/tracking/objects,...` | Comma-separated list of MQTT topics to subscribe to — one per camera |
| `mqtt_client_id` | `camera-adapter` | MQTT client identifier |
