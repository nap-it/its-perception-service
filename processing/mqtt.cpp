#include "mqtt.h"
#include "cpm-processing.h"


server readConfigFile(const string& path)
{
    server data;

    // .ini file reader
    INIReader reader (path);

    // MQTT host name
    string host = reader.Get("mqtt", "host", "localhost");
    cout << "Host: " << host << endl;

    // MQTT port
    long port = reader.GetInteger("mqtt", "port", 1883);

    // Complete address (host name + port)
    data.address = "tcp://" + host + ":" + to_string(port);
    spdlog::info("MQTT address {}", data.address);

    // MQTT topics to subscribe
    string topicsStr = reader.Get("mqtt", "topic_subscribe", "");
    // Split the string by comma to get individual topics
    std::string topic;
    std::istringstream iss(topicsStr);
    while (std::getline(iss, topic, ',')) {
        data.subscription_topic.push_back(topic);
        spdlog::info("Subscription topic {}", topic);
    }

    data.client_id = reader.Get("mqtt", "client_id", "client");
    data.publish_topic = reader.Get("mqtt", "topic_publish", "hello");
    spdlog::info("Publish topic {}", data.publish_topic);

    data.qos= reader.GetInteger("mqtt", "qos", 1);
    data.n_retry_attempts= reader.GetInteger("mqtt", "n_retry_attempts", 5);

    return data;

}

[[noreturn]] void loop_forever ()
{
    while(true) {
        sleep(1);
    };
}

std::function<void(string)> cli_publish;

void mqttSetPublishProcessedCpm(const std::function<void(string)>& f)
{
    cli_publish = f;
}

/* Executed function when a message arrived to the subscribed topic */
void callback::message_arrived(mqtt::const_message_ptr msg) {
    chrono::time_point<chrono::system_clock> start,end;

    start = chrono::system_clock::now();

    // spdlog::info("Message arrived from topic {}", msg->get_topic());

    Document aux, received_cpm;

    if (msg->get_topic().find("in") != std::string::npos) {
        received_cpm.Parse(msg->to_string().c_str());
    }
    else {
        aux.Parse(msg->to_string().c_str());
        received_cpm.CopyFrom(aux["fields"]["cpm"], received_cpm.GetAllocator());
    }

    // cout << "Received CPM: " << endl;
    // cout << msg->to_string() << endl;

    string cpm_objects = process_cpm(received_cpm);
    if (cpm_objects.empty()) {
        return;
    }
    // cli_.publish(data_server.publish_topic, cpm_objects);

    // cout << "CPM objects: " << endl;
    // cout << cpm_objects << endl;

    if(cli_publish) {
        cli_publish(cpm_objects);
    }

    end = chrono::system_clock::now();

    chrono::duration<double> elapsed = end - start;

    spdlog::info("Total time {}", elapsed.count());
    //cout << elapsed.count() << endl;

}

/* The topic subscription  is made when the connection to the MQTT server was successful */
void callback::connected(const std::string &cause) {
    spdlog::info("Connection success");


    for (int i=0; i<data_server.subscription_topic.size(); i++) {
        cli_.subscribe(data_server.subscription_topic.at(i), data_server.qos, nullptr, subListener_);
        spdlog::info("Subscribing to topic {}", data_server.subscription_topic.at(i));
    }

}

void callback::reconnect() {
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    try {
        cli_.connect(connOpts_, nullptr, *this);
    }
    catch (const mqtt::exception& exc) {
        spdlog::error("Error: ", exc.what());
        exit(1);
    }
}

void callback::on_success(const mqtt::token &tok) { }

void callback::on_failure(const mqtt::token &tok) {
    spdlog::warn("Easy padding in numbers like {:08d}", 12);
}

void callback::connection_lost(const std::string &cause) {
    spdlog::warn("Connection lost");
    if (!cause.empty())
        spdlog::warn("Connection lost caused by {}", cause);

    spdlog::info("Reconnecting...");
    /*n_retry_ = 0;*/
    reconnect();

}

void callback::delivery_complete(mqtt::delivery_token_ptr token) {

    spdlog::info("Delivery complete");

}

void action_listener::on_failure(const mqtt::token &tok) {

    if (tok.get_message_id() != 0){
        spdlog::warn("{} failure for token: {}", name_, tok.get_message_id());
    }
}

void action_listener::on_success(const mqtt::token &tok) {
    if (tok.get_message_id() != 0){
        spdlog::info("{} success for token: {}", name_, tok.get_message_id());
    }

    auto top = tok.get_topics();
    if (top && !top->empty()){
        spdlog::info("\ttoken topic: {}", (*top)[0]);
    }

}
