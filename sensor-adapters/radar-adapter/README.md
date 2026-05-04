# Radar Adapter

The Radar Adapter bridges a radar detection pipeline into the CPS Generation service. It subscribes to one or more MQTT topics carrying object detections from a radar, normalises the data into the CPS object format, and publishes it to the DDS topic `generation/objects`.

### Data Flow

```
Radar pipeline → MQTT "<mqtt_topic>" → Radar Adapter → DDS "generation/objects"
```

### Expected Input (MQTT)

Each MQTT message is a flat JSON object describing a single detected object. `sensorID` is hardcoded to `1` (Radar) by the adapter.

```json
{
    "objectID": 42,
    "timestamp": 1746000000.123,
    "classification": 5,
    "confidence": 90,
    "speed": 12.5,
    "heading": 180.0,
    "acceleration": 0.3,
    "latitude": 40.63045,
    "longitude": -8.65410,
    "length": 4.5
}
```

| Field | Required | Description |
|---|---|---|
| `objectID` | yes | Unique object identifier |
| `timestamp` | yes | UNIX timestamp of the detection (seconds) |
| `latitude` | yes | Object latitude (degrees) |
| `longitude` | yes | Object longitude (degrees) |
| `classification` | no | ETSI TrafficParticipantType integer |
| `confidence` | no | Detection confidence (0–100) |
| `speed` | no | Speed (m/s) |
| `heading` | no | Heading (0–360°) |
| `acceleration` | no | Acceleration (m/s²) |
| `length` | no | Object length, mapped to `size_x` (m) |

## `config.ini` Configuration

```ini
[radar-adapter]
domain_id = 0
debug = false
mqtt_host = 127.0.0.1
mqtt_port = 1883
mqtt_topics = jetson/radar-plus
mqtt_client_id = radar-adapter
```

| Key | Default | Description |
|---|---|---|
| `domain_id` | `0` | DDS domain ID — **change this** to match `aggregator.domain_id` in the Generation config |
| `debug` | `false` | Enable debug-level logging |
| `mqtt_host` | `127.0.0.1` | MQTT broker address where radar detections are published |
| `mqtt_port` | `1883` | MQTT broker port |
| `mqtt_topics` | `jetson/radar-plus` | Comma-separated list of MQTT topics to subscribe to — one per radar |
| `mqtt_client_id` | `radar-adapter` | MQTT client identifier |
