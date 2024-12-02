
# CPM Processing

## Output message format
###### The CPM Processing outputs specific DDS/MQTT message formats containing information about all detected objects from the received CPM.

 - /objects

```
[
    {
        id: int,
        uniqueID: int64,
        age: int,
        objectTimestamp: float,
        objectPerceptionQuality: float,
        sensor: string,					// string representation of sensor
        sensorID: int,
        latitude: float,
        longitude: float,
        speed: float,
        acceleration: float,
        heading: float,
		classification: string,			// string representation of classification
		classificationID: int,
        stationSenderID: int,
        stationSenderType: int,
        stationReceiverID: int,
        stationReceiverType: int,
        referenceTimestamp: int64
    },
    (...)
]
```

 - /objects_full

```
[
    {
        id: int,
        uniqueID: int64,
        age: int,
        objectTimestamp: float,
        objectPerceptionQuality: float,
        sensor: string,
        sensorID: int,
        latitude: float,
        longitude: float,
        referenceLatitude: float,
        referenceLongitude: float,
        xDistance: float,				// distance in meters to the reference position (East)
        yDistance: float,				// distance in meters to the reference position (North)
        zDistance: float,				// altitude in meters
        xDistanceCov: float, 			// xDistance covariance
        yDistanceCov: float, 			// yDistance covariance
		zDistanceCov: float, 			// zDistance covariance
		xVelocity: float,				// velocity vector x based on heading
		yVelocity: float,				// velocity vector y based on heading
		xVelocityCov: foat,
		yVelocityCov: float,
        speed: float,
        acceleration: float,
        xAcceleration: float,
        yAcceleration: float,
        zAngularVelocity: float,		// angular velocity
        zAngularVelocityCov: float,
        heading: float,
        headingCov: float,
        size_x: float,					// object length
        size_y: float, 					// object width
        size_z: float, 					// object height
		classification: string,
		classificationID: int,
        stationSenderID: int,
        stationSenderType: int,
        stationReceiverID: int,
        stationReceiverType: int,
        referenceTimestamp: int64
    },
    (...)
]
```
