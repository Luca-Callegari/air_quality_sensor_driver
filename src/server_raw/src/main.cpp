/** 
  * Air Quality main sensor driver
  * 
  * Luca Callegari <luketto.c@gmail.com>
  * Mirko Damiano <mirketto2001.md@gmail.com>
  * 
  * August 4, 2026
*/

#include <iostream>
#include <server_raw/server.hpp>

int main(int argc, char **argv){
  rclcpp::init(argc, argv);

  rclcpp::executors::MultiThreadedExecutor executor;

  auto server_node = std::make_shared<server_components::ReaderNode>(rclcpp::NodeOptions());

  executor.add_node(server_node);

  executor.spin();

  rclcpp::shutdown();
  exit(EXIT_SUCCESS);
}