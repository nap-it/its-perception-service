# Collective Perception Service (CPS)

The Collective Perception Service (CPS) is a C++ V2X communication module implementing the **Collective Perception Service** according to ETSI TS 103 324. It collects object detections from multiple heterogeneous sensors — radars, cameras, and lidar-based platforms — and generates **Collective Perception Messages (CPMs)** that inform nearby vehicles and infrastructure of the detected objects. A companion **Processing** service decodes incoming CPMs and republishes the enriched object data on standard messaging transports for consumption by other applications.

### Citation
If you find this code useful in your research, please consider citing:

```bibtex
@article{FIGUEIREDO2026,
  author  = {Andreia Figueiredo and João Amaral and Pedro Rito and Miguel Luís and Susana Sargento},
  title   = {Improving object selection for Collective Perception Messages under congestion},
  journal = {Ad Hoc Networks},
  year    = {2026},
  doi     = {https://doi.org/10.1016/j.adhoc.2026.104175},
}
```

### Key Features
- ETSI TS 103 324 compliant CPM generation
- Multi-sensor fusion (radar, camera, lidar / Autoware VPI, bike camera)
- ETSI priority-based object freshness evaluation
- Modular sensor adapter architecture — easy to extend with new sensor types
- Flexible station location providers: static (RSU) or dynamic via MQTT/DDS (OBU)
- Multi-transport output: DDS, local MQTT, remote MQTT, Zenoh
- Prometheus metrics for both generation and processing services
- Ansible-based production deployment

## Table of Contents
- [Requirements](#requirements)
- [Quick Start](#quick-start)
- [Architecture](#architecture)
- [How It Works](#how-it-works)
- [Message Formats](#message-formats)
- [Configuration](#configuration)
- [Deployment](#deployment)
- [Sensor Adapters](#sensor-adapters)
- [Monitoring & Metrics](#monitoring--metrics)
- [Development Status](#development-status)
- [Documentation & Examples](#documentation--examples)
- [Authors](#authors)
- [License](#license)

## Requirements

- **MQTT Broker** *(mandatory)*  
  Used by sensor adapters and optionally by the Processing service output.
  ```bash
  sudo apt install mosquitto mosquitto-clients
  sudo systemctl start mosquitto
  ```

- **Docker & Docker Compose** *(mandatory)*  
  Used to run all CPS components.
  ```bash
  docker --version
  docker compose version
  ```

- **V2X Stack (Vanetza-NAP)** *(optional)*  
  Required only for transmitting CPMs over a V2X network.  
  The CPS can run without it — Generation publishes CPMs on DDS and the Processing service can consume them locally.  
  Vanetza-NAP is available at https://github.com/nap-it/vanetza-nap

## Quick Start

1. Clone the repository and enter the project folder:
```bash
git clone <YOUR_REPOSITORY_URL>
cd cps-v2
```

2. Start the CPS (Generation + Processing + Camera Adapter + Radar Adapter):
```bash
docker compose up -d
```

3. Simulate a sensor detection by publishing an object to the camera adapter:
```bash
mosquitto_pub -h localhost -t "jetson/camera/1/tracking/objects" -m '{
  "objects": [{
    "objectID": 1,
    "sensorID": 3,
    "timestamp": 1746000000.0,
    "latitude": 40.6303,
    "longitude": -8.6542,
    "heading": 90.0,
    "speed": 5.0,
    "confidence": 80,
    "classification": 1
  }]
}'
```

4. Observe the processed output:
```bash
mosquitto_sub -h localhost -t "objects" -v
```

## Architecture

![Collective Perception Service Architecture](docs/images/architecture.png)

The CPS is composed of several Docker containers that together form a complete CPM generation and processing pipeline:

### Generation
The core service that produces CPMs. It ingests object and sensor data from Sensor Adapters over DDS or Zenoh, evaluates which objects are fresh enough to be reported (using ETSI priority rules), constructs a compliant CPM JSON, and publishes it via DDS (typically to `vanetza/in/cpm` for V2X transmission).

Internally composed of three classes:
- **Aggregator** — subscribes to `generation/objects` and `generation/sensors`, maintains the object cache, and applies ETSI freshness/priority logic
- **Builder** — stateless CPM JSON constructor; converts the fresh object list and sensor metadata into an ETSI-compliant structure
- **Locator** — supplies the station's current position (static config, or live from MQTT/DDS CAM/VAM messages)

### Processing
Subscribes to DDS CPM topics, decodes each message, enriches objects with absolute coordinates and decomposed velocity vectors, and republishes the result in an application-friendly JSON format on MQTT, DDS, and Zenoh.

### Sensor Adapters
Lightweight bridges that translate sensor-specific data formats into the CPS object format and publish them to the `generation/objects` DDS topic. Available adapters:
- **[Camera Adapter](sensor-adapters/camera-adapter/)** — camera detection pipelines (e.g. Jetson)
- **[Radar Adapter](sensor-adapters/radar-adapter/)** — radar detection pipelines
- **[Bike Adapter](sensor-adapters/bike-adapter/)** — Raspberry Pi camera on a bicycle
- **[Autoware Adapter](sensor-adapters/autoware-adapter/)** — Autoware VPI autonomous driving platform (DDS input)

## How It Works

### CPM Generation Logic

On each generation cycle (default: every 100 ms), the Generation service:

1. Queries the Aggregator for **fresh objects** — objects whose state has changed enough to be included in a new CPM, according to ETSI TS 103 324 priority rules
2. Gets the current station position from the Locator
3. Passes the fresh objects, sensor metadata, and station position to the Builder
4. Publishes the resulting CPM JSON

**ETSI Priority Rules** — an object is considered fresh if any of the following thresholds are exceeded since the object was last included in a CPM:
- Position change ≥ 4 m (P_MIN–P_MAX: 0–8 m)
- Speed change ≥ 0.5 m/s (S_MIN–S_MAX: 0–1 m/s)
- Heading change ≥ 4° (O_MIN–O_MAX: 0–8°)
- Time since last inclusion ≥ 1000 ms (T_MIN–T_MAX: 100–1000 ms)

These thresholds can be bypassed by setting `aggregator.ignore_rules = true` in the Generation config.

### Processing Logic

For each received CPM, the Processing service:
1. Extracts sender metadata from a CAM lookup (speed, heading, altitude, acceleration)
2. Converts ETSI relative x/y distances back to absolute WGS-84 lat/lon coordinates
3. Decomposes object speed and heading into x/y (north/east) velocity components
4. Maps integer sensor type and classification codes to human-readable strings
5. Publishes the enriched object list

## Message Formats

### Sensor Data — input topic `generation/sensors`

```json
{
    "sensorID": 3,
    "sensorType": 3,
    "shadowingApplies": false
}
```

| Field | Type | Description |
|---|---|---|
| `sensorID` | integer | Unique sensor identifier (usually matches sensorType) |
| `sensorType` | integer | ETSI [SensorType](https://forge.etsi.org/rep/ITS/asn1/cpm_ts103324/-/blob/master/docs/ETSI-ITS-CDD.md#SensorType): 1=Radar, 2=Lidar, 3=Monovideo, 12=LocalAggregation, 13=ITSAggregation |
| `shadowingApplies` | boolean | Whether shadowing applies to this sensor |

### Object Data — input topic `generation/objects`

```json
{
    "objects": [
        {
            "objectID": 1,
            "sensorID": 3,
            "timestamp": 1746000000.0,
            "latitude": 40.6303,
            "longitude": -8.6542,
            "altitude": 10.0,
            "heading": 90.0,
            "speed": 5.0,
            "acceleration": 0.0,
            "confidence": 80,
            "classification": 1,
            "size_x": 0.5,
            "size_y": 1.7,
            "size_z": 1.8,
            "angular_velocity": 0.0,
            "cov_heading": 0.0,
            "cov_speed": 0.0,
            "cov_latitude": 0.0,
            "cov_longitude": 0.0,
            "cov_altitude": 0.0,
            "cov_angular_velocity": 0.0
        }
    ]
}
```

Fields marked as optional can be omitted; the Generation service will treat missing values as unavailable.

| Field | Type | Required | Description |
|---|---|---|---|
| `objectID` | integer | yes | Unique object identifier (per sensor) |
| `sensorID` | integer | yes | ID of the sensor that detected this object |
| `timestamp` | double | yes | UNIX timestamp of the detection (seconds) |
| `latitude` | float | yes | Object latitude (degrees) |
| `longitude` | float | yes | Object longitude (degrees) |
| `heading` | float | yes | Object heading (0–360°) |
| `speed` | float | yes | Object speed (m/s) |
| `confidence` | integer | yes | Detection confidence (1–100, or 101 if unavailable) |
| `classification` | integer | yes | [TrafficParticipantType](https://forge.etsi.org/rep/ITS/asn1/cpm_ts103324/-/blob/master/docs/ETSI-ITS-CDD.md#TrafficParticipantType): 0=Unknown, 1=Pedestrian, 2=Bicycle, 3=Moped, 4=Motorcycle, 5=Car, 6=Bus, 7=LightTruck, 8=HeavyTruck, 15=RSU |
| `altitude` | float | no | Object altitude (m) |
| `acceleration` | float | no | Object acceleration (m/s²) |
| `size_x` | float | no | Object width (m) |
| `size_y` | float | no | Object length (m) |
| `size_z` | float | no | Object height (m) |
| `angular_velocity` | float | no | Angular velocity (rad/s) |
| `cov_*` | float | no | Covariance values for the corresponding field |

### Processed Objects — output topic `objects`

Published by the Processing service. See [docs/examples/processed_output.json](docs/examples/processed_output.json) for a full example.

```json
{
    "sender": {
        "id": 1001,
        "type": 15,
        "latitude": 40.6303,
        "longitude": -8.6542,
        "altitude": 10.0,
        "speed": 0.0,
        "acceleration": 0.0,
        "heading": 0.0
    },
    "receiver": {
        "id": 1002,
        "type": 15
    },
    "cpm": {
        "age": 50,
        "timestamp": 1746000000.0,
        "timestamp2004": 694485600.0
    },
    "objects": [
        {
            "id": 1,
            "uniqueID": 10011,
            "age": 50,
            "timestamp": 1746000000.0,
            "classification": "pedestrian",
            "classificationID": 1,
            "confidence": 80,
            "latitude": 40.6305,
            "longitude": -8.6540,
            "xDistance": 25.0,
            "yDistance": 15.0,
            "altitude": 10.0,
            "speed": 5.0,
            "xVelocity": 5.0,
            "yVelocity": 0.0,
            "heading": 90.0,
            "sensorType": "monovideo",
            "sensorID": 3
        }
    ]
}
```

## Configuration

Each container is configured via a `config.ini` file mounted as a Docker volume. Refer to each component's README for the full configuration reference:

- [Generation `config.ini`](generation/README.md)
- [Processing `config.ini`](processing/README.md)
- [Camera Adapter `config.ini`](sensor-adapters/camera-adapter/README.md)
- [Radar Adapter `config.ini`](sensor-adapters/radar-adapter/README.md)
- [Bike Adapter `config.ini`](sensor-adapters/bike-adapter/README.md)
- [Autoware Adapter `config.ini`](sensor-adapters/autoware-adapter/README.md)

## Deployment

### Docker Compose (local / development)

The [`docker-compose.yml`](docker-compose.yml) file includes the Generation, Processing, Camera Adapter, and Radar Adapter services:

```bash
docker compose up -d
```

Additional compose files are available for single-camera and radar-only deployments under the [`agent/`](agent/) folder.

### Ansible (production)

The [`deployment/`](deployment/) folder contains an Ansible role that deploys the CPS to one or more target hosts.

```bash
cd deployment
ansible-playbook deploy.yml -i inventory/hosts.yml
```

Edit [`deployment/inventory/hosts.yml`](deployment/inventory/hosts.yml) to set the target host(s) and override any configuration variables. The full list of configurable variables is in [`deployment/roles/cps/defaults/main.yml`](deployment/roles/cps/defaults/main.yml).

Supported deployment scenarios (defined in `hosts.yml`): RSU, PIXKIT, HOLOLENS, BIKE, and others.

## Sensor Adapters

Sensor Adapters are the integration point between sensor-specific pipelines and the CPS. Each adapter:
1. Receives detections from a sensor pipeline (via MQTT or DDS)
2. Normalises the data into the CPS `generation/objects` JSON format
3. Publishes to the DDS topic `generation/objects`

### Implementing a Custom Adapter

To integrate a new sensor type, create an adapter that publishes a JSON message to the DDS topic `generation/objects` (domain ID matching `aggregator.domain_id` in the Generation config) in the format described in the [Object Data](#object-data----input-topic-generationobjects) section above.

Optionally, also publish to `generation/sensors` to include a Sensor Information Container in the CPM.

The existing adapters ([camera](sensor-adapters/camera-adapter/), [radar](sensor-adapters/radar-adapter/)) are minimal and serve as practical templates.

## Monitoring & Metrics

When Prometheus metrics are enabled (`prometheus = true` in config), the services expose metrics endpoints:

| Service | Port | Config flag |
|---|---|---|
| Generation | 9102 | `generation.prometheus = true` |
| Processing | 9103 | `processing.prometheus = true` |

**Generation metrics** (`cps_generation_*`):
- `cpm_publish_total` — total CPMs published
- `last_cpm_ts_seconds` — UNIX timestamp of the last CPM
- `cpm_build_ms` — CPM build latency histogram
- `cycle_ms` — generation loop cycle duration histogram
- `objects_per_msg` — objects per CPM histogram
- `radar_messages_total` — total radar object messages received
- `camera_messages_total` — total camera object messages received

**Processing metrics** (`cps_processing_*`):
- `object_age_ms` — received object age histogram
- `cycle_ms` — processing cycle duration histogram
- `pub_messages_total` — total messages published

Performance CSV logs (when `performance_logs = true`) are written to `/logs/` inside the container.

## Development Status

### Implemented
- CPM generation with ETSI TS 103 324 compliant structure
- ETSI priority-based object freshness evaluation
- Movement predictor priority mode (alternative to ETSI rules)
- Management Container and Perceived Object Container
- Sensor Information Container
- Multi-sensor support (radar, camera, lidar/Autoware, bike camera)
- Station location providers: STATIC, MQTT (CAM/VAM), DDS (CAM/VAM)
- Multi-transport output: DDS, local MQTT, remote MQTT, Zenoh
- Prometheus metrics
- Ansible deployment

## Documentation & Examples

- **Component configuration:** See each component's `README.md` for `config.ini` parameter reference
- **CPM structure reference:** See [docs/cpmReference.md](docs/cpmReference.md) for a field-by-field breakdown of the CPM JSON format
- **Input examples:**
  - [docs/examples/sensor_object_input.json](docs/examples/sensor_object_input.json) — object data published by a sensor adapter
  - [docs/examples/sensor_info_input.json](docs/examples/sensor_info_input.json) — sensor metadata published by a sensor adapter
- **Output examples:**
  - [docs/examples/cpm_output.json](docs/examples/cpm_output.json) — synthetic CPM produced by the Generation service
  - [docs/examples/cpm_output_captured.json](docs/examples/cpm_output_captured.json) — real captured CPM (from Autoware VPI / ITS aggregation sensor)
  - [docs/examples/cpm_received_v2x.json](docs/examples/cpm_received_v2x.json) — real CPM as received over V2X (with radio metadata)
  - [docs/examples/processed_output.json](docs/examples/processed_output.json) — enriched objects published by the Processing service

## Authors

Development of the CPS is part of ongoing research work at [Instituto de Telecomunicações' Network Architectures and Protocols Group](https://www.it.pt/Groups/Index/36).

## License

The Collective Perception Service is licensed under LGPLv3. See the [LICENSE](LICENSE) file for details.
