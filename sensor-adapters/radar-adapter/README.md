# Radar Adapter

## MQTT topic expected message format
###### The Radar Adapter expects a specific MQTT message format containing information about each detected object.

```
{
    objectID: int,
    receiverID: int,
    acceleration: float,
    speed: float,
    classification: int,
    confidence: int,
    heading: float,
    latitude: float,
    longitude: float,
    length: float,
    timestamp: double
}
```
