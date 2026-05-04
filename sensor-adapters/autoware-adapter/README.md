# Autoware Adapter

The Autoware Adapter bridges the [Autoware VPI](https://github.com/nap-it/aw-vpi) autonomous driving platform into the CPS Generation service. Unlike the camera and radar adapters, this adapter uses **DDS as its input transport** — it subscribes to a DDS topic carrying object detections from Autoware VPI and re-publishes them to the `generation/objects` DDS topic consumed by the Generation service.

### Data Flow

```
Autoware VPI → DDS "<autoware_topic>" → Autoware Adapter → DDS "generation/objects"
```

### Expected Input (DDS)

Each DDS message is a JSON object with an `objects` array. `sensorID` is hardcoded to `2` (Lidar) by the adapter. Autoware VPI provides the richest set of fields including covariances and altitude. Timestamps are provided in **nanoseconds** and are converted to seconds by the adapter.

```json
{
    "objects": [
        {
            "objID": 7,
            "timestamp": 1746000000123000000,
            "classification": 5,
            "confidence": 95,
            "speed": 8.3,
            "cov_speed": 0.01,
            "heading": 45.0,
            "cov_heading": 0.02,
            "latitude": 40.63045,
            "longitude": -8.65410,
            "cov_y": 0.001,
            "cov_x": 0.001,
            "z": 12.5,
            "cov_z": 0.1,
            "size_x": 4.5,
            "size_y": 1.9,
            "size_z": 1.5,
            "twist_angz": 0.05,
            "cov_twist_angz": 0.001
        }
    ]
}
```

| Field | Required | Description |
|---|---|---|
| `objects[].objID` | yes | Unique object identifier |
| `objects[].timestamp` | yes | Detection timestamp in **nanoseconds** (converted to seconds internally) |
| `objects[].latitude` | yes | Object latitude (degrees) |
| `objects[].longitude` | yes | Object longitude (degrees) |
| `objects[].classification` | no | ETSI TrafficParticipantType integer |
| `objects[].confidence` | no | Detection confidence (0–100) |
| `objects[].speed` | no | Speed (m/s) |
| `objects[].heading` | no | Heading (0–360°) |
| `objects[].z` | no | Altitude (m), mapped to `altitude` |
| `objects[].size_x/y/z` | no | Object dimensions (m) |
| `objects[].twist_angz` | no | Angular velocity (rad/s) |
| `objects[].cov_x/y/z` | no | Position covariances (longitude, latitude, altitude) |
| `objects[].cov_speed` | no | Speed covariance |
| `objects[].cov_heading` | no | Heading covariance |
| `objects[].cov_twist_angz` | no | Angular velocity covariance |

## `config.ini` Configuration

```ini
[autoware-adapter]
debug = false
domain_id = 0
```

| Key | Default | Description |
|---|---|---|
| `domain_id` | `0` | DDS domain ID — **change this** to match the Autoware VPI DDS domain |
| `debug` | `false` | Enable debug-level logging |
