# Bike Adapter

The Bike Adapter bridges a Raspberry Pi camera mounted on a bicycle into the CPS Generation service. It subscribes to an MQTT topic carrying object detections from the bike camera pipeline, normalises the data into the CPS object format, and publishes it to the DDS topic `generation/objects`.

### Data Flow

```
RPi camera pipeline → MQTT "<mqtt_topic>" → Bike Adapter → DDS "generation/objects"
```

### Expected Input (MQTT)

Each MQTT message must be a JSON object with a `detections` array. `sensorID` is hardcoded to `3` (Monovideo) by the adapter. Note the bike pipeline uses shorter field names (`lat`/`lon`, `object_id`, `class`) compared to other adapters.

```json
{
    "detections": [
        {
            "object_id": 1,
            "timestamp": 1746000000.123,
            "class": 1,
            "confidence": 75,
            "lat": 40.63045,
            "lon": -8.65410
        }
    ]
}
```

| Field | Required | Description |
|---|---|---|
| `detections[].object_id` | yes | Unique object identifier |
| `detections[].lat` | yes | Object latitude (degrees) |
| `detections[].lon` | yes | Object longitude (degrees) |
| `detections[].timestamp` | no | UNIX timestamp of the detection (seconds) |
| `detections[].class` | no | ETSI TrafficParticipantType integer |
| `detections[].confidence` | no | Detection confidence (0–100) |

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

| Key | Default | Description | Range |
|---|---|---|---|
| `domain_id` | `0` | DDS domain ID — **change this** to match `aggregator.domain_id` in the Generation config | `0` to `230` |
| `debug` | `false` | Enable debug-level logging | `true/false` |
| `mqtt_host` | `127.0.0.1` | MQTT broker address where bike camera detections are published | - |
| `mqtt_port` | `1883` | MQTT broker port | - |
| `mqtt_topic` | `rpi/cam_detections` | MQTT topic to subscribe to | - |
| `mqtt_client_id` | `bike-adapter` | MQTT client identifier | - |
