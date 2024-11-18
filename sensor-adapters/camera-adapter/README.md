# Camera Adapter

## MQTT topic expected message format
###### The Camera Adapter expects a specific MQTT message format containing information about all detected objects.

```
{
    timestamp: double,
    listOfObjects: [
        {
            objectID: int,
            classification: int;
            confidence: int,
            latitude: float,
            longitude: float,
            heading: float,
            speed: float
        },
        {
            objectID: int,
            classification: int;
            confidence: int,
            latitude: float,
            longitude: float,
            heading: float,
            speed: float
        },
        (...)
    ]
}
```
