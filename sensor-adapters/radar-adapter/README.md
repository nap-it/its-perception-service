# Radar Adapter

## MQTT topic expected message format
###### The Radar Adapter expects a specific MQTT message format containing information about each detected object.

```
{
    "acceleration": float,
    "classification": int,
    "confidence": int,
    "heading": float,
    "latitude": float,
    "longitude": float,
    "length": float,
    "cloudPersist": boolean,
    "objectID": int,
    "receiverID": int,
    "speed": float,
    "timestamp": float,
}
```
