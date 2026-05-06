# Camera Adapter

The Camera Adapter bridges a camera detection pipeline into the CPS Generation service. It subscribes to one or more MQTT topics carrying object detections from a camera (e.g. a Jetson-based tracking pipeline), normalises the data into the CPS object format, and publishes it to the DDS topic `generation/objects`.

### Data Flow

```
Camera pipeline → MQTT "<mqtt_topic>" → Camera Adapter → DDS "generation/objects"
```

### Expected Input (MQTT)

Each MQTT message must be a JSON object with a top-level `timestamp` and a `listOfObjects` array. `sensorID` is hardcoded to `3` (Monovideo) by the adapter.

```json
{
    "timestamp": 1746000000.123,
    "listOfObjects": [
        {
            "objectID": 1,
            "classification": 1,
            "confidence": 80,
            "speed": 1.4,
            "heading": 90.0,
            "latitude": 40.63045,
            "longitude": -8.65410
        }
    ]
}
```

| Field | Location | Required | Description |
|---|---|---|---|
| `timestamp` | top-level | yes | UNIX timestamp of the detection batch (seconds) |
| `listOfObjects[].objectID` | per object | yes | Unique object identifier |
| `listOfObjects[].latitude` | per object | yes | Object latitude (degrees) |
| `listOfObjects[].longitude` | per object | yes | Object longitude (degrees) |
| `listOfObjects[].classification` | per object | no | ETSI TrafficParticipantType integer |
| `listOfObjects[].confidence` | per object | no | Detection confidence (0–100) |
| `listOfObjects[].speed` | per object | no | Speed (m/s) |
| `listOfObjects[].heading` | per object | no | Heading (0–360°) |

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

| Key | Default | Description | Range |
|---|---|---|---|
| `domain_id` | `0` | DDS domain ID — **change this** to match `aggregator.domain_id` in the Generation config | `0` to `230` |
| `debug` | `false` | Enable debug-level logging | `true/false` |
| `mqtt_host` | `127.0.0.1` | MQTT broker address where camera detections are published | - |
| `mqtt_port` | `1883` | MQTT broker port | - |
| `mqtt_topics` | `jetson/camera/1/tracking/objects,...` | Comma-separated list of MQTT topics to subscribe to — one per camera | - |
| `mqtt_client_id` | `camera-adapter` | MQTT client identifier | - |
