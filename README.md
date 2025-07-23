# Collective Perception Service

## Architecture

![Collective Perception Service](CPS.png)[drawio](https://drive.google.com/file/d/1vUR5Nu4ZPlkBnMAqkcHEop4V3RriTGb3/view?usp=sharing)

The CPS consists of several components deployed as Docker containers:
- **[Generation](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/tree/main/generation)**: The main service that handles the logic of gathering object and sensor data from **Sensor Adapters** using **MQTT** or **Zenoh** and generating **Collective Perception Messages (CPMs)** according to the *ETSI* CPM specification.

- **[Processing](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/tree/main/processing)**: The service that processes incoming CPMs, extracts all relevant information and publishes it using **MQTT**, **DDS** and **Zenoh** on two different topics:
    - `objects`: Contains all objects from the CPMs on a simplified smaller format.
    - `objects_full`: Contains all objects from the CPMs on a full format.

    It also provides the option to publish the topics in a remote **MQTT** broker, such as *ATCLL*.

- **Sensor Adapters**: These are the components that gather data from sensors and publish it to the **Generation** service using either **DDS** or **Zenoh** in a *specific format*. So far, the following adapters were implemented:
    - **[Camera Adapter](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/tree/main/sensor-adapters/camera-adapter)**: Gathers data from cameras. 
    - **[Radar Adapter](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/tree/main/sensor-adapters/radar-adapter)**: Gathers data from radars.
    - **[Bike Adapter](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/tree/main/sensor-adapters/bike-adapter)**: Gathers data from the bike RPI camera.
    - **[Autoware Adapter](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/tree/main/sensor-adapters/autoware-adapter)**: Gathers data from the [Autoware VPI](https://code.nap.av.it.pt/adas/aw-vpi).


## Message Formats
The Sensor Adapters publish both sensor and object data in a specific format that is understood by the **Generation** service. The following formats are used:
#### Sensor Data Format for the topic `generation/sensors`

```json
{
    "sensorID": integer, // Unique identifier for the sensor (usually the same as Type)
    "sensorType": integer, // Type of the sensor
    "shadowingApplies": boolean, // Whether shadowing applies to the sensor, usually false
}
```

The [`sensorType`](https://forge.etsi.org/rep/ITS/asn1/cpm_ts103324/-/blob/master/docs/ETSI-ITS-CDD.md#SensorType) is an integer that represents the type of sensor, such as:
- 0: Undefined
- 1: Radar
- 2: Lidar
- 3: Monovideo (camera)
- 12: Local Aggregation
- 13: ITS Aggregation 

#### Object Data Format for the topic `generation/objects`

```json
{
    "objects": [
        {
            "objectID": integer, // Unique identifier for the object
            "sensorID": integer, // ID of the sensor that detected this object
            "timestamp": double, //UNIX timestamp of the detection in seconds
            "latitude": float, // Latitude of the object in degrees
            "longitude": float, // Longitude of the object in degrees
            "altitude": float, // [OPTIONAL] Altitude of the object in meters
            "heading": float, // Heading of the object in degrees (0-360)
            "speed": float, // Speed of the object in m/s
            "acceleration": float, // [OPTIONAL] Acceleration of the object in m/s²
            "confidence": integer, // Confidence level of the detection (0-100)
            "classification": integer, // Classification of the object 
            "size_x": float, // [OPTIONAL] Size of the object in the x-axis (width) in meters
            "size_y": float, // [OPTIONAL] Size of the object in the y-axis (length) in meters
            "size_z": float, // [OPTIONAL] Size of the object in the z-axis (height) in meters
            "angular_velocity": float, // [OPTIONAL] Angular velocity of the object in rad/s
            "cov_heading": float, // [OPTIONAL] Covariance of the heading
            "cov_speed": float, // [OPTIONAL] Covariance of the speed 
            "cov_latitude": float, // [OPTIONAL] Covariance of the latitude
            "cov_longitude": float, // [OPTIONAL] Covariance of the longitude
            "cov_altitude": float, // [OPTIONAL] Covariance of the altitude
            "cov_angular_velocity": float // [OPTIONAL] Covariance of the angular velocity
        }
        {...}
    ]
}
```

The `classification` ([TrafficParticipantType](https://forge.etsi.org/rep/ITS/asn1/cpm_ts103324/-/blob/master/docs/ETSI-ITS-CDD.md#TrafficParticipantType)) is an integer that represents the type of object, such as:
- 0: Undefined
- 1: Pedestrian
- 2: Bicycle
- 3: Moped
- 4: Motorcycle
- 5: Car
- 6: Bus
- 7: Light Truck
- 8: Heavy Truck
- 9: Trailer
- 10: Special Vehicle
- 11: Tram
- 12: light VRU Vehicle (e.g., scooter, skateboard, etc.)
- 13: Animal
- 14: Agricultural Vehicle
- 15: Roadside Unit (RSU)



## Deployment

The file [`deployment/inventory/hosts.yml`](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/blob/main/deployment/inventory/hosts.yml) contains an series of examples of how to deploy the CPS on different scenarios, such as RSU, PIXKIT, HOLOLENS, BIKE, etc.

#### Run the following command inside the deployment folder:

```bash
ansible-playbook deploy.yml -i inventory/hosts.yml
```

To change the target, edit the [`deployment/inventory/hosts.yml`](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/blob/main/deployment/inventory/hosts.yml).

#### Configure variables:
The full list of configurable variables can be found under ***cps_configs*** in the [`deployment/roles/cps/defaults/main.yml`](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/blob/main/deployment/roles/cps/defaults/main.yml) file. 
To override any of these variables, you can add a ***cps_overrides*** section in any host of the [`deployment/inventory/hosts.yml`](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/blob/main/deployment/inventory/hosts.yml) file.

## Usage
The CPS is launchable using **Docker Compose**. The [`docker-compose.yml`](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/blob/main/docker-compose.yml) file contains an example of the necessary services to run the CPS with Radar and Camera sensors. 

```bash
docker compose up -d
```

## Configuration
The CPS was designed to be easily configurable and adaptable to different scenarios. Each of the different docker containers can be configured uisng a `config.ini`file which is mounted as a volume in the container:
- [Generation `config.ini` variables](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/blob/main/generation)
- [Processing `config.ini` variables](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/blob/main/processing)
- [Camera Adapter `config.ini` variables](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/blob/main/sensor-adapters/camera-adapter)
- [Radar Adapter `config.ini` variables](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/blob/main/sensor-adapters/radar-adapter)
- [Bike Adapter `config.ini` variables](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/blob/main/sensor-adapters/bike-adapter)
- [Autoware Adapter `config.ini` variables](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/blob/main/sensor-adapters/autoware-adapter)

## Token

Private token: `glpat-BZYHmcoyr2u-Bsx1sFoZ`
