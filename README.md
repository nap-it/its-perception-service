# Collective Perception Service

## Architecture

![Collective Perception Service](CPS.png)[drawio](https://drive.google.com/file/d/1vUR5Nu4ZPlkBnMAqkcHEop4V3RriTGb3/view?usp=sharing)

The CPS consists of several components deployed as Docker containers:
- **[Generation](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/tree/main/generation)**: The main service that handles the logic of gathering object and sensor data from **Sensor Adapters** using **MQTT** or **Zenoh** and generating **Collective Perception Messages (CPMs)** according to the *ETSI* CPM specification.

- **[Processing](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/tree/main/processing)**: The service that processes incoming CPMs, extracts all relevant information and publishes it using **MQTT**, **DDS** and **Zenoh** on two different topics:
    - `objects`: Contains all objects from the CPMs on a simplified smaller format.
    - `objects_full`: Contains all objects from the CPMs on a full format.

    It also provides the option to publish the topics in a remote **MQTT** broker, such as *ATCLL*.

- **Sensor Adapters**: These are the components that gather data from sensors and publish it to the **Generation** service using either **DDS** or **Zenoh**. Until now, the following adapters were implemented:
    - **[Camera Adapter](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/tree/main/sensor-adapters/camera-adapter)**: Gathers data from cameras. 
    - **[Radar Adapter](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/tree/main/sensor-adapters/radar-adapter)**: Gathers data from radars.
    - **[Bike Adapter](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/tree/main/sensor-adapters/bike-adapter)**: Gathers data from the bike RPI camera.
    - **[Autoware Adapter](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/tree/main/sensor-adapters/autoware-adapter)**: Gathers data from the [Autoware VPI](https://code.nap.av.it.pt/adas/aw-vpi).

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
