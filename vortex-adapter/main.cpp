//REQUIREMENTS

/*
    CAM topic: necessary to get the observer's position and speed
    Objects topic: necessary to get the objects' positions and speeds
    [config.ini]
        dds_domain_id: Fast-DDS domain ID (related to the OBU domain ID)
        debug_level: 0 for info, 1 for debug
        reference_latitude: latitude of the reference point
        reference_longitude: longitude of the reference point
        cam_topic: topic to subscribe to get the observer's position and speed
        objects_topic: topic to subscribe to get the objects' positions and speeds
*/

#include "rclcpp/rclcpp.hpp"

//libraries
#include <chrono>
#include <cmath>
#include <numbers>
#include <boost/asio.hpp>
#include <fstream>
#include <thread>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Matrix3x3.h>

//local libraries
#include "config_reader.hpp"
#include <spdlog/spdlog.h>
#include "fastdds-cpp-wrapper/dds.hpp"
#include <rapidjson/document.h>

//messages
#include "vortex_perception_msgs/msg/observer.hpp"
#include "vortex_perception_msgs/msg/object.hpp"
#include "std_msgs/msg/header.hpp"

using std::placeholders::_1;
using namespace rapidjson;
using namespace std;

//config variables
int domain_id = 0;
int debug_level = 0;
double reference_latitude = 0;
double reference_longitude = 0;
string cam_topic = "";
string objects_topic = "";
string publish_topic = "";

//observer variables
double obs_latitude = 0;
double obs_longitude = 0;
double obs_x = 0;
double obs_y = 0;
float obs_qx = 0;
float obs_qy = 0;
float obs_qz = 0;
float obs_qw = 0;
float obs_speed = 0;
float obs_heading = 0;
float obs_heading_rate = 0;
float obs_size_x = 0;
float obs_size_y = 0;
uint32_t obs_id = 0;
uint32_t obs_type = 0;

//objects variables
std::vector<vortex_perception_msgs::msg::Object> objects;

const double earth_radius = 6371000;

std::mutex objects_mutex;

Dds* dds_;

class ObserverPublisher : public rclcpp::Node
{
public:
ObserverPublisher()
: Node("VortexAdapter"){

    //QoS should be set to depth = 10, durability = transient_local
    rmw_qos_profile_t custom_qos = rmw_qos_profile_default;
    custom_qos.depth = 10;
    custom_qos.durability = RMW_QOS_POLICY_DURABILITY_TRANSIENT_LOCAL;
    auto qos = rclcpp::QoS(rclcpp::QoSInitialization::from_rmw(custom_qos), custom_qos);

    observer_pub = this->create_publisher<vortex_perception_msgs::msg::Observer>(publish_topic, qos);
    timer_ = this->create_wall_timer(100ms, std::bind(&ObserverPublisher::publishLoop, this));

}


private:
    void publishLoop() {

        //Create observer message and fill it with the observer's data and the objects' data at 10Hz

        auto msg = vortex_perception_msgs::msg::Observer();
        msg.id = obs_id;
        msg.type = 1;
        msg.header.stamp = this->now();
        msg.header.frame_id = "map";

        msg.gnss.vector.y = obs_latitude;
        msg.gnss.vector.x = obs_longitude;

        spdlog::debug("Observer gnss position: {}, {}", msg.gnss.vector.x, msg.gnss.vector.y);

        msg.pose.pose.position.x = obs_x;
        msg.pose.pose.position.y = obs_y;
        msg.pose.pose.position.z = 0;

        spdlog::debug("Observer position: {}, {}", msg.pose.pose.position.x, msg.pose.pose.position.y);

        msg.pose.covariance[0] = 0.0001;
        msg.pose.covariance[7] = 0.0001;

        msg.heading = obs_heading;

        msg.pose.pose.orientation.x = obs_qx;
        msg.pose.pose.orientation.y = obs_qy;
        msg.pose.pose.orientation.z = obs_qz;
        msg.pose.pose.orientation.w = obs_qw;

        spdlog::debug("Observer orientation: {}, {}, {}, {}", msg.pose.pose.orientation.x, msg.pose.pose.orientation.y, msg.pose.pose.orientation.z, msg.pose.pose.orientation.w);

        msg.size.vector.x = obs_size_x;
        msg.size.vector.y = obs_size_y;
        msg.size.vector.z = 1.5;

        spdlog::debug("Observer size: {}, {}, {}", msg.size.vector.x, msg.size.vector.y, msg.size.vector.z);

        

        msg.dynamics.speed = obs_speed;
        msg.dynamics.velocity.twist.linear.x = obs_speed * cos(obs_heading);
        msg.dynamics.velocity.twist.linear.y = obs_speed * sin(obs_heading);
        msg.dynamics.velocity.covariance[0] = 0.0001;
        msg.dynamics.velocity.covariance[7] = 0.0001;
        msg.dynamics.velocity.covariance[35] = 0.001;

        spdlog::debug("Observer speed: {}", msg.dynamics.speed);
        spdlog::debug("Observer velocity: {}, {}", msg.dynamics.velocity.twist.linear.x, msg.dynamics.velocity.twist.linear.y);

        msg.objects.clear();

        {
            std::lock_guard<std::mutex> lock(objects_mutex);
            for(const auto& obj : objects){
                msg.objects.push_back(obj);
            }
        }

        observer_pub->publish(msg);
        spdlog::info("Published observer message");

    }

    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Publisher<vortex_perception_msgs::msg::Observer>::SharedPtr observer_pub;

};

float degrees_to_radians(float degrees) {
    return degrees * (M_PI / 180.0);
}

void heading_to_quaternion(float heading_angle, float& qx, float& qy, float& qz, float& qw) {
    // Compute yaw in degrees
    float yaw_in_degrees = 90.0 - heading_angle;

    // Normalize yaw_in_degrees to be between -180 and 180
    if (yaw_in_degrees > 180.0)
        yaw_in_degrees -= 360.0;
    if (yaw_in_degrees < -180.0)
        yaw_in_degrees += 360.0;

    // Convert yaw to radians
    float yaw_in_radians = degrees_to_radians(yaw_in_degrees);

    // Assuming roll and pitch are zero
    double roll = 0.0;
    double pitch = 0.0;

    tf2::Quaternion q;
    q.setRPY(roll, pitch, yaw_in_radians);

    qx = q.x();
    qy = q.y();
    qz = q.z();
    qw = q.w();
}

void calculate_distance(double reference_lat, double reference_lon, double obs_lat, double obs_lon){
    double lat1 = degrees_to_radians(reference_lat);
    double lon1 = degrees_to_radians(reference_lon);
    double lat2 = degrees_to_radians(obs_lat);
    double lon2 = degrees_to_radians(obs_lon);

    double dlat = lat2 - lat1;
    double dlon = lon2 - lon1;

    double X = earth_radius * cos((lat1 + lat2) / 2) * dlon;
    double Y = earth_radius * dlat;
    
    obs_x = X;
    obs_y = Y;
    spdlog::info("X: {}, Y: {}", obs_x, obs_y);
}

void computeNewXY(double x, double y,
                  double latA_deg, double lonA_deg,
                  double latB_deg, double lonB_deg,
                  double& x_prime, double& y_prime)
{
    // Earth's radius in meters
    const double R = 6371000.0; // Mean Earth radius

    // Convert degrees to radians
    double latA_rad = latA_deg * M_PI / 180.0;
    double lonA_rad = lonA_deg * M_PI / 180.0;
    double latB_rad = latB_deg * M_PI / 180.0;
    double lonB_rad = lonB_deg * M_PI / 180.0;

    // Differences in coordinates
    double delta_lat_rad = latB_rad - latA_rad;
    double delta_lon_rad = lonB_rad - lonA_rad;

    // Mean latitude in radians
    double mean_lat_rad = (latA_rad + latB_rad) / 2.0;

    // North and East displacements in meters between A and B
    double delta_N = delta_lat_rad * R; // North displacement
    double delta_E = delta_lon_rad * R * cos(mean_lat_rad); // East displacement

    spdlog::info("Delta N: {}, Delta E: {}", delta_N, delta_E);

    // Adjust the original coordinates
    x_prime = x - delta_E;
    y_prime = y - delta_N;
}

void on_message_dds(string topic, string message) {
    spdlog::info("Received DDS message on topic {}", topic);
    spdlog::info("Message: {}", message);

    Document d;
    d.Parse(message.c_str());

    if(topic == objects_topic) {
        spdlog::info("Received objects message");
        //number of objects
        int n_objects = d.Size();
        spdlog::debug("Number of objects: {}", n_objects);

        {
            std::lock_guard<std::mutex> lock(objects_mutex);

            objects.clear();
            for (auto& obj : d.GetArray()){
                vortex_perception_msgs::msg::Object object;
                object.id = obj["id"].GetUint();
                spdlog::debug("Object ID: {}", object.id);

                object.header.stamp.sec = static_cast<int>(obj["objectTimestamp"].GetDouble());
                object.header.frame_id = "map";

                double obj_x = obj["xDistance"].GetDouble();
                double obj_y = obj["yDistance"].GetDouble();
                double obj_z = obj["zDistance"].GetDouble();
                double obj_x_cov = obj["xDistanceCov"].GetDouble();
                double obj_y_cov = obj["yDistanceCov"].GetDouble();
                double obj_z_cov = obj["zDistanceCov"].GetDouble();
                obj_x_cov = obj_x_cov * obj_x_cov;
                obj_y_cov = obj_y_cov * obj_y_cov;
                obj_z_cov = obj_z_cov * obj_z_cov;
                double obj_ref_lat = obj["referenceLatitude"].GetDouble();
                double obj_ref_lon = obj["referenceLongitude"].GetDouble();

                double x;
                double y;

                // spdlog::debug("Computing new x and y");
                // spdlog::debug("Object x: {}, Object y: {}", obj_x, obj_y);
                // spdlog::debug("Object ref lat: {}, Object ref lon: {}", obj_ref_lat, obj_ref_lon);
                // spdlog::debug("Reference lat: {}, Reference lon: {}", reference_latitude, reference_longitude);

                computeNewXY(obj_x, obj_y, obj_ref_lat, obj_ref_lon, reference_latitude, reference_longitude, x, y);

                // spdlog::debug("New x: {}, New y: {}", x, y);

                object.pose.pose.position.x = x;
                object.pose.pose.position.y = y;
                object.pose.pose.position.z = obj_z;
                spdlog::debug("Object x: {}, y: {}, z: {}", object.pose.pose.position.x, object.pose.pose.position.y, object.pose.pose.position.z);

                object.pose.covariance[0] = obj_x_cov;
                object.pose.covariance[7] = obj_y_cov;
                object.pose.covariance[14] = obj_z_cov;
                spdlog::debug("Object x cov: {}, y cov: {}, z cov: {}", object.pose.covariance[0], object.pose.covariance[7], object.pose.covariance[14]);

                float heading = obj["heading"].GetFloat();
                float heading_cov = obj["headingCov"].GetFloat();
                heading_cov = heading_cov * heading_cov;
                // spdlog::debug("Object heading: {} and cov {}", heading, heading_cov);
                float qx, qy, qz, qw;
                heading_to_quaternion(heading, qx, qy, qz, qw);
                
                object.pose.pose.orientation.x = qx;
                object.pose.pose.orientation.y = qy;
                object.pose.pose.orientation.z = qz;
                object.pose.pose.orientation.w = qw;
                spdlog::debug("Object qx: {}, qy: {}, qz: {}, qw: {}", object.pose.pose.orientation.x, object.pose.pose.orientation.y, object.pose.pose.orientation.z, object.pose.pose.orientation.w);

                object.pose.covariance[35] = heading_cov;
                spdlog::debug("Object heading cov: {}", object.pose.covariance[35]);

                vortex_perception_msgs::msg::Classification classification;
                classification.sensor_type = 2;
                int classification_id = obj["classificationID"].GetInt();

                if(classification_id == 1) classification.class_name = "person";
                else if(classification_id == 5) classification.class_name = "car";

                classification.score = 1.0;

                spdlog::debug("Object classification: {}", classification.class_name);

                object.classification.push_back(classification);

                float size_x = obj["size_x"].GetFloat();
                float size_y = obj["size_y"].GetFloat();
                float size_z = obj["size_z"].GetFloat();

                object.size.vector.x = size_x;
                object.size.vector.y = size_y;
                object.size.vector.z = size_z;

                spdlog::debug("Object size: x: {}, y: {}, z: {}", object.size.vector.x, object.size.vector.y, object.size.vector.z);

                object.dynamics.speed = obj["speed"].GetFloat();
                
                object.dynamics.velocity.twist.linear.x = obj["xVelocity"].GetFloat();
                float obj_x_vel_cov = obj["xVelocityCov"].GetFloat();
                object.dynamics.velocity.covariance[0] = obj_x_vel_cov * obj_x_vel_cov;
                
                object.dynamics.velocity.twist.linear.y = obj["yVelocity"].GetFloat();
                float obj_y_vel_cov = obj["yVelocityCov"].GetFloat();
                object.dynamics.velocity.covariance[7] = obj_y_vel_cov * obj_y_vel_cov;

                spdlog::debug("Object x velocity: {}, y velocity: {}", object.dynamics.velocity.twist.linear.x, object.dynamics.velocity.twist.linear.y);
                spdlog::debug("Object x velocity cov: {}, y velocity cov: {}", object.dynamics.velocity.covariance[0], object.dynamics.velocity.covariance[7]);
                spdlog::debug("Object speed: {}", object.dynamics.speed);

                float obj_z_ang_vel = obj["zAngularVelocity"].GetFloat();
                obj_z_ang_vel = obj_z_ang_vel * (M_PI / 180.0);
                object.dynamics.velocity.twist.angular.z = obj_z_ang_vel;
                float obj_z_ang_vel_cov = obj["zAngularVelocityCov"].GetFloat();
                object.dynamics.velocity.covariance[35] = obj_z_ang_vel_cov * obj_z_ang_vel_cov;

                spdlog::debug("Object z angular velocity: {}, cov: {}", object.dynamics.velocity.twist.angular.z, object.dynamics.velocity.covariance[35]);


                int object_stationSenderID = obj["stationSenderID"].GetInt();

                if(object_stationSenderID == obs_id) objects.push_back(object);
            }
        }

    } else if(topic == cam_topic) {
        spdlog::info("Received cam message");
        if (topic == "vanetza/out/cam_full"){
            Document out_cam;
            out_cam.Parse(message.c_str());

            obs_id = out_cam["fields"]["header"]["stationID"].GetInt();

            if(out_cam.HasMember("fields") && out_cam["fields"].HasMember("cam")){
                obs_type = out_cam["fields"]["cam"]["camParameters"]["basicContainer"]["stationType"].GetInt();
                obs_latitude = out_cam["fields"]["cam"]["camParameters"]["basicContainer"]["referencePosition"]["latitude"].GetDouble();
                obs_longitude = out_cam["fields"]["cam"]["camParameters"]["basicContainer"]["referencePosition"]["longitude"].GetDouble();

                obs_speed = out_cam["fields"]["cam"]["camParameters"]["highFrequencyContainer"]["basicVehicleContainerHighFrequency"]["speed"]["speedValue"].GetFloat();
                obs_heading = out_cam["fields"]["cam"]["camParameters"]["highFrequencyContainer"]["basicVehicleContainerHighFrequency"]["heading"]["headingValue"].GetFloat();
                obs_heading_rate = out_cam["fields"]["cam"]["camParameters"]["highFrequencyContainer"]["basicVehicleContainerHighFrequency"]["yawRate"]["yawRateValue"].GetFloat();

                //calculate distance between observer and reference point in meters
                calculate_distance(reference_latitude, reference_longitude, obs_latitude, obs_longitude);
                //convert heading to quaternion
                heading_to_quaternion(obs_heading, obs_qx, obs_qy, obs_qz, obs_qw);

                obs_size_x = out_cam["fields"]["cam"]["camParameters"]["highFrequencyContainer"]["basicVehicleContainerHighFrequency"]["vehicleLength"]["vehicleLengthValue"].GetFloat();
                obs_size_y = out_cam["fields"]["cam"]["camParameters"]["highFrequencyContainer"]["basicVehicleContainerHighFrequency"]["vehicleWidth"].GetFloat();

            }
        } else if (topic == "vanetza/in/cam_full"){

            //static data for testing
            obs_id = 229;
            obs_type = 5;

            Document cam;
            cam.Parse(message.c_str());
            obs_latitude = cam["camParameters"]["basicContainer"]["referencePosition"]["latitude"].GetDouble();
            obs_longitude = cam["camParameters"]["basicContainer"]["referencePosition"]["longitude"].GetDouble();

            //calculate distance between observer and reference point in meters
            calculate_distance(reference_latitude, reference_longitude, obs_latitude, obs_longitude);
            obs_heading = cam["camParameters"]["highFrequencyContainer"]["basicVehicleContainerHighFrequency"]["heading"]["headingValue"].GetFloat();
            obs_heading_rate = cam["camParameters"]["highFrequencyContainer"]["basicVehicleContainerHighFrequency"]["yawRate"]["yawRateValue"].GetFloat();
            obs_speed = cam["camParameters"]["highFrequencyContainer"]["basicVehicleContainerHighFrequency"]["speed"]["speedValue"].GetFloat();

            //convert heading to quaternion
            heading_to_quaternion(obs_heading, obs_qx, obs_qy, obs_qz, obs_qw);

            obs_size_x = cam["camParameters"]["highFrequencyContainer"]["basicVehicleContainerHighFrequency"]["vehicleLength"]["vehicleLengthValue"].GetFloat();
            obs_size_y = cam["camParameters"]["highFrequencyContainer"]["basicVehicleContainerHighFrequency"]["vehicleWidth"].GetFloat();
        }
    }
}

void readConfigFile(const string& path){
    INIReader reader (path);

    if (reader.ParseError() < 0) {
        spdlog::error("Can't load config file");
        return;
    }

    domain_id = reader.GetInteger("vortex-adapter", "dds_domain_id", 175);
    cout << "[Vortex - Config] Fast-DDS Wrapper Domain ID: " << domain_id << endl;
    debug_level = reader.GetInteger("vortex-adapter", "debug_level", 1);
    cout << "[Vortex - Config] Debug Level: " << debug_level << endl;
    reference_latitude = reader.GetReal("vortex-adapter", "reference_latitude", 0);
    cout << "[Vortex - Config] Reference Latitude: " << reference_latitude << endl;
    reference_longitude = reader.GetReal("vortex-adapter", "reference_longitude", 0);
    cout << "[Vortex - Config] Reference Longitude: " << reference_longitude << endl;
    cam_topic = reader.Get("vortex-adapter", "cam_topic", "vanetza/out/cam");
    cout << "[Vortex - Config] CAM Topic: " << cam_topic << endl;
    objects_topic = reader.Get("vortex-adapter", "objects_topic", "objects");
    cout << "[Vortex - Config] Objects Topic: " << objects_topic << endl;
    publish_topic = reader.Get("vortex-adapter", "publish_topic", "observer");
    cout << "[Vortex - Config] Publish Topic: " << publish_topic << endl;

}

int main(int argc, char * argv[]) {
    

    cout << "Starting Vortex-adapter ..." << endl;
    readConfigFile("/vortex_adapter/config.ini");
    if(debug_level){
        spdlog::set_level(spdlog::level::debug);
    } else {
        spdlog::set_level(spdlog::level::info);
    }

    rclcpp::init(argc, argv);
    auto node = std::make_shared<ObserverPublisher>();
    rclcpp::executors::MultiThreadedExecutor executor;
    executor.add_node(node);
    std::thread executor_thread([&executor]() { executor.spin(); });

    //DDS
    cout << "Setting up DDS..." << endl;
    dds_ = new Dds("VortexAdapter", domain_id, on_message_dds);
    dds_->subscribe(cam_topic);
    dds_->subscribe(objects_topic);
    cout << "DDS set up" << endl;

    while (rclcpp::ok()) {

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // If rclcpp::ok() returns false, it means ros2 has been shutdown, 
    // so join the executor thread before closing the application
    if (executor_thread.joinable()) {
        executor_thread.join();
    }

    rclcpp::shutdown();

    return 0;
}