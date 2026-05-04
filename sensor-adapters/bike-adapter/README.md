# Bike Adapter

The Bike Adapter bridges a Raspberry Pi camera mounted on a bicycle into the CPS Generation service. It subscribes to an MQTT topic carrying object detections from the bike camera pipeline, normalises the data into the CPS object format, and publishes it to the DDS topic `generation/objects`.

### Data Flow

```
RPi camera pipeline → MQTT "<mqtt_topic>" → Bike Adapter → DDS "generation/objects"
```

### Expected Input (MQTT)

Each MQTT message must be a JSON object containing an `objects` array. Each element must include at minimum: `objectID`, `sensorID`, `timestamp`, `latitude`, `longitude`, `heading`, `speed`, `confidence`, and `classification`. Optional fields not provided by the bike camera pipeline (`altitude`, `size_*`, covariances) default to `0.0`.

See the root [README.md](../../README.md#object-data----input-topic-generationobjects) for the full field reference.

## `config.ini` Configuration

```ini
[bike-adapter]
domain_id = 0
debug = false
mqtt_host = 127.0.0.1
mqtt_port = 1883
mqtt_topic = rpi/cam_detections
mqtt_client_id = bike-adapter
```

| Key | Default | Description |
|---|---|---|
| `domain_id` | `0` | DDS domain ID — **change this** to match `aggregator.domain_id` in the Generation config |
| `debug` | `false` | Enable debug-level logging |
| `mqtt_host` | `127.0.0.1` | MQTT broker address where bike camera detections are published |
| `mqtt_port` | `1883` | MQTT broker port |
| `mqtt_topic` | `rpi/cam_detections` | MQTT topic to subscribe to |
| `mqtt_client_id` | `bike-adapter` | MQTT client identifier |
