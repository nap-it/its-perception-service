# CPM Structure Reference

This document describes the JSON structure of a **Collective Perception Message (CPM)** as produced by the CPS Generation service. The format is designed to be consumed directly by [Vanetza-NAP](https://github.com/nap-it/vanetza-nap), which handles ASN.1 UPER encoding and V2X transmission.

> **Note:** The CPS does **not** apply ETSI unit scaling. All values are in SI units (meters, m/s, degrees, seconds). Unit scaling is delegated to Vanetza-NAP.

**ETSI Reference:** ETSI TS 103 324 — Intelligent Transport Systems (ITS); Vehicular Communications; Basic Set of Applications; Specification of the Collective Perception Service (CPS)

---

## Top-Level Structure

```json
{
    "managementContainer": { ... },
    "cpmContainers": [ ... ]
}
```

| Field | Type | Description |
|---|---|---|
| `managementContainer` | object | Mandatory. Contains the reference time and position of the originating station. |
| `cpmContainers` | array | List of CPM containers. Always includes the station container (ID 1 or 2) and the Perceived Object Container (ID 5). Optionally includes the Sensor Information Container (ID 3). |

---

## Management Container

```json
"managementContainer": {
    "referenceTime": 694485600.123,
    "referencePosition": {
        "latitude": 40.63028,
        "longitude": -8.65422,
        "altitude": {
            "altitudeValue": 0.0,
            "altitudeConfidence": 0
        },
        "positionConfidenceEllipse": {
            "semiMajorConfidence": 4095,
            "semiMinorConfidence": 4095,
            "semiMajorOrientation": 0
        }
    }
}
```

| Field | Type | Unit | Description |
|---|---|---|---|
| `referenceTime` | double | seconds since 2004-01-01T00:00:00Z | CPM generation timestamp (ETSI ITS epoch) |
| `referencePosition.latitude` | double | degrees (WGS-84) | Originating station latitude |
| `referencePosition.longitude` | double | degrees (WGS-84) | Originating station longitude |
| `referencePosition.altitude.altitudeValue` | double | meters | Originating station altitude. Fixed at `0.0` (not sourced from a GPS fix in this implementation) |
| `referencePosition.altitude.altitudeConfidence` | integer | — | ETSI altitude confidence code. Fixed at `0` (unavailable) |
| `referencePosition.positionConfidenceEllipse.semiMajorConfidence` | integer | cm | Semi-major axis of position confidence ellipse. Fixed at `4095` (unavailable) |
| `referencePosition.positionConfidenceEllipse.semiMinorConfidence` | integer | cm | Semi-minor axis. Fixed at `4095` (unavailable) |
| `referencePosition.positionConfidenceEllipse.semiMajorOrientation` | integer | decidegrees | Orientation of semi-major axis. Fixed at `0` |

---

## CPM Containers Array

The `cpmContainers` array always contains the following entries (in order):

| `containerId` | Name | Condition |
|---|---|---|
| `1` | OBU Station Container | When `locator.station_type = 5` |
| `2` | RSU Station Container | When `locator.station_type = 15` |
| `3` | Sensor Information Container | When at least one sensor has published to `generation/sensors` |
| `5` | Perceived Object Container | Always |

---

## Container ID 1 — OBU Station Container

Included when the originating station is an On-Board Unit (OBU, station type 5).

```json
{
    "containerId": 1,
    "containerData": {
        "orientationAngle": {
            "value": 90.0,
            "confidence": 1
        }
    }
}
```

| Field | Type | Unit | Description |
|---|---|---|---|
| `orientationAngle.value` | float | degrees (0–360°) | Heading of the originating vehicle |
| `orientationAngle.confidence` | integer | — | ETSI heading confidence code |

---

## Container ID 2 — RSU Station Container

Included when the originating station is a Road-Side Unit (RSU, station type 15). The container data is empty.

```json
{
    "containerId": 2,
    "containerData": {}
}
```

---

## Container ID 3 — Sensor Information Container

Included when sensor metadata has been received on the `generation/sensors` topic.

```json
{
    "containerId": 3,
    "containerData": [
        {
            "sensorId": 3,
            "sensorType": 3,
            "shadowingApplies": false
        }
    ]
}
```

| Field | Type | Description |
|---|---|---|
| `sensorId` | integer | Unique sensor identifier |
| `sensorType` | integer | ETSI [SensorType](https://forge.etsi.org/rep/ITS/asn1/cpm_ts103324/-/blob/master/docs/ETSI-ITS-CDD.md#SensorType): 0=Undefined, 1=Radar, 2=Lidar, 3=Monovideo, 4=Stereovision, 5=Nightvision, 6=Ultrasonic, 12=LocalAggregation, 13=ITSAggregation |
| `shadowingApplies` | boolean | Whether sensor shadowing applies (typically `false`) |

---

## Container ID 5 — Perceived Object Container

Contains all objects that passed the ETSI freshness/priority evaluation in the current cycle.

```json
{
    "containerId": 5,
    "containerData": {
        "numberOfPerceivedObjects": 2,
        "perceivedObjects": [ ... ]
    }
}
```

| Field | Type | Description |
|---|---|---|
| `numberOfPerceivedObjects` | integer | Number of objects in `perceivedObjects` |
| `perceivedObjects` | array | List of Perceived Object entries (see below) |

### Perceived Object

```json
{
    "objectId": 1,
    "sensorIdList": [3],
    "measurementDeltaTime": 123,
    "position": { ... },
    "velocity": { ... },
    "acceleration": { ... },
    "angles": { ... },
    "zAngularVelocity": { ... },
    "classification": [ ... ],
    "objectDimensionX": { ... },
    "objectDimensionY": { ... },
    "objectDimensionZ": { ... }
}
```

#### objectId

Integer (1–255). A stable identifier assigned by the Aggregator to track the same physical object across consecutive CPMs. IDs are recycled when objects leave the cache.

#### sensorIdList

Array of integers. The sensor IDs that detected this object. Currently always a single-element array matching the `sensorID` from the input data.

#### measurementDeltaTime

Integer (milliseconds). Time between the CPM reference time and the object detection timestamp. Clamped to the range [−2048, 2047] ms as per ETSI TS 103 324. A value of `−2048` indicates the delta was out of range.

#### position

Relative position of the object from the originating station, using a flat-Earth approximation.

```json
"position": {
    "xCoordinate": { "value": 10.5, "confidence": 40.95 },
    "yCoordinate": { "value": 15.3, "confidence": 40.95 },
    "zCoordinate": { "value": 12.5, "confidence": 40.95 }
}
```

| Field | Type | Unit | Notes |
|---|---|---|---|
| `xCoordinate.value` | double | meters (east) | Eastward displacement from the station |
| `xCoordinate.confidence` | double | meters | Position confidence. Derived from `cov_longitude` (if provided) via √(cov). Clamped to [0, 40.95]. Default: `40.95` (unavailable) |
| `yCoordinate.value` | double | meters (north) | Northward displacement from the station |
| `yCoordinate.confidence` | double | meters | Derived from `cov_latitude`. Same clamping as above |
| `zCoordinate.value` | double | meters | Altitude. **Optional** — only present when `altitude` was provided by the sensor adapter |
| `zCoordinate.confidence` | double | meters | Derived from `cov_altitude`. Default: `40.95` |

#### velocity

Cartesian velocity of the object in the north/east plane. **Optional** — only present when both `speed` and `heading` were provided.

```json
"velocity": {
    "cartesianVelocity": {
        "xVelocity": { "value": 0.0, "confidence": 0.32 },
        "yVelocity": { "value": 1.4, "confidence": 0.32 }
    }
}
```

| Field | Type | Unit | Notes |
|---|---|---|---|
| `xVelocity.value` | double | m/s (east) | `speed × cos(heading)`. Clamped to [−163, 163] m/s |
| `yVelocity.value` | double | m/s (north) | `speed × sin(heading)`. Clamped to [−163, 163] m/s |
| `xVelocity.confidence` | double | m/s | Derived from `cov_speed` via √(cov). Default: `1.27` (unavailable) |
| `yVelocity.confidence` | double | m/s | Same value as `xVelocity.confidence` |

#### acceleration

Cartesian acceleration of the object. **Optional** — only present when both `acceleration` and `heading` were provided.

```json
"acceleration": {
    "cartesianAcceleration": {
        "xAcceleration": { "value": 0.0, "confidence": 1.3 },
        "yAcceleration": { "value": 0.1, "confidence": 1.3 }
    }
}
```

| Field | Type | Unit | Notes |
|---|---|---|---|
| `xAcceleration.value` | double | m/s² (east) | `acceleration × cos(heading)`. Clamped to [−16, 16] m/s² |
| `yAcceleration.value` | double | m/s² (north) | `acceleration × sin(heading)`. Clamped to [−16, 16] m/s² |
| `xAcceleration.confidence` | double | m/s² | Fixed at `1.3` |
| `yAcceleration.confidence` | double | m/s² | Fixed at `1.3` |

#### angles

Object heading. **Optional** — only present when `heading` was provided.

```json
"angles": {
    "zAngle": { "value": 90.0, "confidence": 12.7 }
}
```

| Field | Type | Unit | Notes |
|---|---|---|---|
| `zAngle.value` | float | degrees (0–360°) | Object heading |
| `zAngle.confidence` | double | degrees | Derived from `cov_heading` via √(cov). Clamped to [0, 12.7]. Default: `12.7` (unavailable) |

#### zAngularVelocity

**Optional** — only present when `angular_velocity` was provided.

```json
"zAngularVelocity": { "value": -15, "confidence": 7 }
```

| Field | Type | Unit | Notes |
|---|---|---|---|
| `value` | integer | degrees/s | `angular_velocity × (180/π)`. Clamped to [−255, 256] |
| `confidence` | integer | — | Fixed at `7` |

#### classification

```json
"classification": [
    {
        "objectClass": { "vehicleSubClass": 1 },
        "confidence": 80
    }
]
```

| Field | Type | Notes |
|---|---|---|
| `objectClass.vehicleSubClass` | integer | ETSI [TrafficParticipantType](https://forge.etsi.org/rep/ITS/asn1/cpm_ts103324/-/blob/master/docs/ETSI-ITS-CDD.md#TrafficParticipantType): 0=Unknown, 1=Pedestrian, 2=Bicycle, 3=Moped, 4=Motorcycle, 5=Car, 6=Bus, 7=LightTruck, 8=HeavyTruck, 9=Trailer, 10=SpecialVehicle, 11=Tram, 12=LightVRU, 13=Animal, 14=AgriculturalVehicle, 15=RSU |
| `confidence` | integer | Detection confidence (1–100). Value `101` means unavailable |

#### objectDimensionX / Y / Z

**Optional** — only present when the corresponding size field was provided by the sensor adapter.

```json
"objectDimensionX": { "value": 0.5, "confidence": 32 },
"objectDimensionY": { "value": 1.7, "confidence": 32 },
"objectDimensionZ": { "value": 1.8, "confidence": 32 }
```

| Field | Type | Unit | Notes |
|---|---|---|---|
| `value` | float | meters | X = width, Y = length, Z = height. Clamped to [0.1, 25.0] m. Values below 0.1 are set to 0.5 (minimum meaningful size) |
| `confidence` | integer | — | Fixed at `32` |

---

## Complete Examples

- [examples/cpm_output.json](examples/cpm_output.json) — synthetic CPM with two perceived objects (camera sensor)
- [examples/cpm_output_captured.json](examples/cpm_output_captured.json) — real captured CPM from an Autoware VPI pipeline (ITS aggregation sensor, containerId 3 with perception region)
- [examples/cpm_received_v2x.json](examples/cpm_received_v2x.json) — real CPM as received over a V2X radio link, including radio-layer metadata (RSSI, channel info)
