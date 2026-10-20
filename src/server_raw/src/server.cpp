/** 
  * Air Quality server sensor driver
  * 
  * Luca Callegari <luketto.c@gmail.com>
  * Mirko Damiano <mirketto2001.md@gmail.com>
  * 
  * August 4, 2026
*/

#include <iostream>
#include <chrono>
#include <server_raw/server.hpp>

using SetBool = std_srvs::srv::SetBool;

// Node compilation as a component
namespace server_components
{

ReaderNode::ReaderNode(const rclcpp::NodeOptions & node_opts)
: Node("reader_node", node_opts),
  stop_thread_{false}
{
  // Publishers
  pub_raw_ = this->create_publisher<std_msgs::msg::String>("~/readings_raw", rclcpp::QoS(10));
  pub_ = this->create_publisher<std_msgs::msg::String>("~/readings", rclcpp::QoS(10));

  // Initialize parameters
  pmanager_ = std::make_shared<params_manager::Manager>(this, false);
  init_parameters();

  // Service enable/disable
  srv_set_ = this->create_service<SetBool>(
    "~/enable",
    std::bind(
      &ReaderNode::service_clbk,
      this,
      std::placeholders::_1,
      std::placeholders::_2
    )
  );

  RCLCPP_INFO(this->get_logger(), "Service ready to receive requests");

  // Starts sampling from the constructor, after every other entity has been created
  if (autostart_) {
    enable();
  }
}

ReaderNode::~ReaderNode()
{
  RCLCPP_INFO(this->get_logger(), "Destructor activated");
  disable();
}

void ReaderNode::service_clbk(
  const SetBool::Request::SharedPtr request,
  const SetBool::Response::SharedPtr response
)
{
  if (request->data) {
    response->success = enable();
    response->message = response->success ? "ReaderNode ENABLED" : "ReaderNode enable FAILED";
  } else {
    disable();
    response->success = true;
    response->message = "ReaderNode DISABLED";
  }
}

bool ReaderNode::enable()
{

  // Protects against concurrent enable/disable calls
  std::lock_guard<std::mutex> lock(state_mutex_);

  // Already running
  if (sampling_thread_.joinable()) {
    RCLCPP_WARN(this->get_logger(), "Node already enabled");
    return true;
  }

  // Create and configure serial object, then open the port
  try {
    serial_port_.setPort(serial_port_path_);
    serial_port_.setBaudrate(static_cast<uint32_t>(baudrate_)); //! SERIAL.H TYPE IS UINT32
    serial::Timeout timeout = serial::Timeout::simpleTimeout(200);
    serial_port_.setTimeout(timeout);
    serial_port_.open();
  } catch (const std::exception & e) {
    RCLCPP_ERROR(this->get_logger(), "Opening port FAILURE: %s", e.what());
    return false;
  }

  RCLCPP_INFO(this->get_logger(), "Serial port opening SUCCESS");

  // Start the dedicated thread
  stop_thread_ = false;
  sampling_thread_ = std::thread(&ReaderNode::thread_routine, this);

  RCLCPP_INFO(this->get_logger(), "Sampling thread started");
  return true;
}

void ReaderNode::disable()
{
  std::lock_guard<std::mutex> lock(state_mutex_);

  // Ask the thread to terminate and wait for it
  stop_thread_ = true;
  if (sampling_thread_.joinable()) {
    sampling_thread_.join();
    RCLCPP_INFO(this->get_logger(), "Sampling thread joined");
  }

  // Close serial port and release its resources
  try {
    if (serial_port_.isOpen()) {
      serial_port_.close();
      RCLCPP_INFO(this->get_logger(), "Serial port closing SUCCESS");
    }
  } catch (const std::exception & e) {
    RCLCPP_ERROR(this->get_logger(), "Error while closing serial port: %s", e.what());
  }
}

void ReaderNode::thread_routine()
{

  // Check dedicated thread ID
  RCLCPP_INFO_STREAM(this->get_logger(), "Dedicated Thread ID: " << std::this_thread::get_id());

  serial_error = false;
  while (!stop_thread_ && rclcpp::ok()) {

    // Re-open serial port iff IOException and SerialException happen
    if(serial_error){
      try{

        if (serial_port_.isOpen()) {
          serial_port_.close();
        }

        serial_port_.setPort(serial_port_path_);
        serial_port_.setBaudrate(static_cast<uint32_t>(baudrate_));
        serial_port_.open();

        RCLCPP_INFO(this->get_logger(), "Serial port re-opened SUCCESSFULLY!");
        serial_error = false;
      }
      catch(const std::exception& e){

        RCLCPP_WARN(
          this->get_logger(),
          "Re-open failed again: %s", e.what()
        );
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        continue;
      }

    }

    try {

      // Blocking read, returns after the 200 ms timeout even if no data is available,
      // so the stop flag is checked frequently and join() terminates quickly
      std::string line = serial_port_.readline();

      // Skip empty lines
      if (line.empty()) {
        continue;
      }

      // Discard lines containing null characters
      if (line.find('\0') != std::string::npos) {
        RCLCPP_WARN(this->get_logger(), "Null character detected in line, discarding...");
        continue;
      }

      // Publish to ~/readings_raw
      std_msgs::msg::String new_msg_raw{};
      new_msg_raw.set__data(line);
      pub_raw_->publish(new_msg_raw);

      // Publish to ~/readings
      try {
        auto data_struct = process_sample(line);
        auto data_processed = SampleResultToString(data_struct);
        std_msgs::msg::String new_msg{};
        new_msg.set__data(data_processed);
        pub_->publish(new_msg);

        RCLCPP_INFO(this->get_logger(), "Raw and post-processed messages publish SUCCESS");
      }
      catch (const json::parse_error & e) {
        RCLCPP_WARN(this->get_logger(), "Malformed JSON string, line discarded: %s", e.what());
      }
      catch (const json::out_of_range & e) {
        RCLCPP_WARN(this->get_logger(), "Missing required field in JSON: %s", e.what());
      }
      catch (const json::type_error & e) {
        RCLCPP_WARN(this->get_logger(), "Wrong field type in JSON: %s", e.what());
      }
    }
    catch (const serial::IOException & e) {
      RCLCPP_ERROR(this->get_logger(), "Serial port IO Error: %s", e.what());
      serial_error = true;
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    catch (const serial::SerialException & e) {
      RCLCPP_ERROR(this->get_logger(), "Serial port Error: %s", e.what());
      serial_error = true;
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  }
}

SampleResult ReaderNode::process_sample(std::string & line)
{
  json j = json::parse(line);

  // Extract required fields from JSON
  SampleResult res{};
  res.t_ms = j.at("Time_ms");
  res.pm10 = j.at("PM_A");
  res.pm25 = j.at("PM_B");
  res.pm1 = j.at("PM_C");
  double WE = j.at("CO_WE");
  double AUX = j.at("CO_AUX");
  double voc_raw = j.at("VOC");
  double ch4_v = j.at("CH4");

  double delta = (WE - AUX) * 1000;
  res.co = (delta / conv_CO_) / 1000;
  res.voc = (voc_raw * m_VOC_ + q_VOC_) * RF_VOC_;
  res.ch4 = 177.65 * std::pow(((5 - ch4_v) / ch4_v / RO_CH4_), -2.56);

  return res;
}

std::string ReaderNode::SampleResultToString(SampleResult & res)
{
  json j;
  j["t_ms"] = res.t_ms;
  j["pm10"] = res.pm10;
  j["pm25"] = res.pm25;
  j["pm1"] = res.pm1;
  j["co"] = res.co;
  j["voc"] = res.voc;
  j["ch4"] = res.ch4;

  return j.dump();
}

} // Namespace server_components

#include <rclcpp_components/register_node_macro.hpp>
RCLCPP_COMPONENTS_REGISTER_NODE(server_components::ReaderNode)