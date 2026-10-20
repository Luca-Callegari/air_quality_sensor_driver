/** 
  * Air Quality server sensor driver
  * 
  * Luca Callegari <luketto.c@gmail.com>
  * Mirko Damiano <mirketto2001.md@gmail.com>
  * 
  * August 4, 2026
*/

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <rclcpp/rclcpp.hpp>
#include <serial/serial.h>
#include <nlohmann/json.hpp>
#include <std_msgs/msg/string.hpp>
#include <params_manager_cpp/params_manager.hpp>
#include <std_srvs/srv/set_bool.hpp>

// Node compilation as a component
namespace server_components
{

// Let's make things a little bit simpler here
using json = nlohmann::json;
using SetBool = std_srvs::srv::SetBool;

// Post-processing parameters struct
struct SampleResult{
    long t_ms;
    double pm10;
    double pm25;
    double pm1;
    double co;
    double voc;
    double ch4;
};

/**
 * @brief ReaderNode server node
*/
class ReaderNode : public rclcpp::Node
{
  public:
  
    /**
     * @brief ReaderNode constructor.
     * 
     * Creates publishers, parameters and the enable service. If the "autostart"
     * parameter is true, it calls enable() at the end of the constructor.
     * 
     * @param node_opts Configuration options for the ROS 2 node.
     */
    ReaderNode(const rclcpp::NodeOptions & node_opts);

    /**
     * @brief Destructor for the ReaderNode.
     * 
     * Calls disable() to terminate and join the sampling thread and close the serial port.
     */
    ~ReaderNode();

  private:

    /**
     * @brief Enable procedure.
     * 
     * Configures and opens the serial port, then creates the dedicated sampling thread.
     * Called by the enable service or at the end of the constructor if "autostart" is true.
     * 
     * @return true if the node is running after the call, false if the serial port
     * could not be opened.
     */
    bool enable();

    /**
     * @brief Disable procedure.
     * 
     * Asks the sampling thread to terminate, joins it, and only then closes the serial port
     * releasing its resources. Called by the enable service (data = false) or by the destructor.
     */
    void disable();

    /**
     * @brief Sampling thread routine.
     * 
     * Reads a sample from the serial port, post-processes it, and publishes both the raw
     * and the processed samples. Runs until 'stop_thread_' becomes true.
     */
    void thread_routine();

    /**
     * @brief Callback for the enable/disable service.
     * 
     * Calls enable() or disable() depending on the "data" field of the request.
     * 
     * @param request Message for enable/disable
     * @param response Response message
     */
    void service_clbk(
      const SetBool::Request::SharedPtr request,
      const SetBool::Response::SharedPtr response
    );

    /**
     * @brief Parses a raw JSON string from the serial port and processes sensor readings.
     * 
     * Extracts raw data, applying sensor-specific calibration formulas to calculate final concentration values.
     * 
     * @param line Raw input JSON string received from the serial communication.
     * 
     * @return SampleResult Struct containing post-processed sensor parameters.
     */
    SampleResult process_sample(std::string& line);

    /**
     * @brief Serializes a SampleResult struct into a JSON string.
     * 
     * @param res Reference to the SampleResult struct containing post-processed data.
     * 
     * @return std::string JSON string.
     */
    std::string SampleResultToString(SampleResult& res);

    /**
     * Smart pointer to the generated parameter manager instance.
     * 
     * Handles fetching, validation, and updates of ROS2 node parameters 
     */
    std::shared_ptr<params_manager::Manager> pmanager_;
    void init_parameters();

    // Dedicated sampling thread, created in enable() and joined in disable()
    std::thread sampling_thread_;

    // Set to true to ask the sampling thread to terminate
    std::atomic<bool> stop_thread_;

    // Serializes enable()/disable() calls
    std::mutex state_mutex_;

    // Serial port object
    serial::Serial serial_port_;

    // Set to true when, during thread routine, an IOException and SerialException happen
    bool serial_error;

    // Publishers
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr pub_raw_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr pub_;

    // Service enable/disable
    rclcpp::Service<std_srvs::srv::SetBool>::SharedPtr srv_set_;

    // Raw data parameters
    bool autostart_;
    double conv_CO_;
    double m_VOC_;
    double q_VOC_;
    double RF_VOC_;
    double RO_CH4_;

    int64_t baudrate_;
    std::string serial_port_path_;
};

} // Namespace server_components