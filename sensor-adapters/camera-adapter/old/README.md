# Camera Adapter

## MQTT topic expected message format
###### The Camera Adapter expects a specific MQTT message format containing information about all detected objects.

```
{
    "numberOf": int,
    "listOfObjects": [
        {
        "objectID": int,
        "globalID": int,
        "classification": int,
        "confidence": int,
        "bbox": {
            "top_left_x": int,
            "top_left_y": int,
            "width": int,
            "height": int
        },
        "latitude": float,
        "longitude": float,
        "heading": null,
        "speed": null,
        "event": ""
    },
    {
        "objectID": int,
        "globalID": int,
        "classification": int,
        "confidence": int,
        "bbox": {
            "top_left_x": int,
            "top_left_y": int,
            "width": int,
            "height": int
        },
        "latitude": float,
        "longitude": float,
        "heading": null,
        "speed": null,
        "event": ""
    }
    ],
    "timestamp": float,
    "receiverID": int,
    "test": {
            "timestamp_start_yolo": float,
            "timestamp_end_yolo": float
    }
}
```

