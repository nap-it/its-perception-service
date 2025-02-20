# CPM Generation
## The CPM Generation makes a request with the following message format
```
{
    "requestID": int,
    "numberObjects": int            //optional (legacy)
}
```

## DDS adapter response topic expected message format
###### The CPM Generation expects a specific DDS response format containing information about all detected objects from the adapter with the correct requestID

```
{
    "requestID: int,                //must be same as request
    "numberObjects: int,            //optional (legacy)
    "objects":[
        {
            objID: int,
            timestamp: double,
            latitude: float,
            longitude: float,
            heading: float,
            speed: float,
            acceleration: float,
            confidence: int,
            sensorID: int,
            classification: [
                {
                    objectClass: {
                        vehicleSubClass: int
                    },
                    confidence: int
                }
            ],
            z: float,                   //optional altitude
            cov_heading: float,         //optional heading covariance
            cov_speed: float,           //optional speed covariance
            size_x: float,              //optional object x dimension
            size_y: float,              //optional object y dimension
            size_z: float,              //optional object z dimension
            cov_x: float,               //optional object x position covariance
            cov_y: float,               //optional object y position covariance
            cov_z: float,               //optional object z position covariance
            twist_angz: float,          //optional angular velocity
            cov_twist_angz: float,      //optional angular velocity covariance
        },
        {
            objID: int,
            timestamp: double,
            latitude: float,
            longitude: float,
            heading: float,
            speed: float,
            acceleration: float,
            confidence: int,
            sensorID: int,
            classification: [
                {
                    objectClass: {
                        vehicleSubClass: int
                    },
                    confidence: int
                }
            ],
            z: float,                   //optional altitude
            cov_heading: float,         //optional heading covariance
            cov_speed: float,           //optional speed covariance
            size_x: float,              //optional object x dimension
            size_y: float,              //optional object y dimension
            size_z: float,              //optional object z dimension
            cov_x: float,               //optional object x position covariance
            cov_y: float,               //optional object y position covariance
            cov_z: float,               //optional object z position covariance
            twist_angz: float,          //optional angular velocity
            cov_twist_angz: float,      //optional angular velocity covariance
        },
        (...)
    ]
}
```
