# Autoware Adapter

The Autoware Adapter bridges the [Autoware VPI](https://github.com/nap-it/aw-vpi) autonomous driving platform into the CPS Generation service. Unlike the camera and radar adapters, this adapter uses **DDS as its input transport** — it subscribes to a DDS topic carrying object detections from Autoware VPI and re-publishes them to the `generation/objects` DDS topic consumed by the Generation service.

### Data Flow

```
Autoware VPI → DDS "<autoware_topic>" → Autoware Adapter → DDS "generation/objects"
```

### Expected Input (DDS)

Each DDS message must be a JSON object containing an `objects` array. Each element must include at minimum: `objectID`, `sensorID`, `timestamp`, `latitude`, `longitude`, `heading`, `speed`, `confidence`, `classification`, and `size_x`. Optional fields (`altitude`, `size_y`, `size_z`, covariances) default to `0.0`.

See the root [README.md](../../README.md#object-data----input-topic-generationobjects) for the full field reference.

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
