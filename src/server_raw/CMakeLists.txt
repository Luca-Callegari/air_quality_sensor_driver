cmake_minimum_required(VERSION 3.8)
project(server_raw)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
# Uncomment the following section in order to fill in
# further dependencies manually.
# find_package(<dependency> REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(params_manager_cpp REQUIRED)
find_package(std_srvs REQUIRED)

find_library(SERIAL_LIB serial REQUIRED)
find_path(SERIAL_INCLUDE_DIR serial/serial.h REQUIRED)

include_directories(
  include
  ${SERIAL_INCLUDE_DIR}
)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/src")

# Generate parameters source code
generate_init_parameters(
  YAML_FILE "config/params.yaml"
  OUT_FILE "src/init_parameters.cpp")

# Server component
add_library(server SHARED
  src/server.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/src/init_parameters.cpp
)

target_compile_definitions(server PRIVATE COMPOSITION_BUILDING_DLL)
target_include_directories(server PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${SERIAL_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/include
  )

ament_target_dependencies(server
  rclcpp
  rclcpp_components
  std_msgs
  std_srvs
  params_manager_cpp
  )

target_link_libraries(server
  ${SERIAL_LIB}
  nlohmann_json::nlohmann_json
  params_manager_cpp::paramsmanager
  )

rclcpp_components_register_nodes(server "server_components::ReaderNode")

# Server node

add_executable(main_cpp 
  src/main.cpp 
)

target_compile_features(main_cpp PUBLIC c_std_99 cxx_std_17)

target_include_directories(main_cpp PRIVATE
  ${SERIAL_INCLUDE_DIR}
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  )

ament_target_dependencies(main_cpp
  rclcpp
  rclcpp_components
  std_msgs
  std_srvs
  )

target_link_libraries(main_cpp
  server
  )

# Installation rules

# Install components
install(TARGETS server
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

# Install executables and other files
install(TARGETS main_cpp
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY config include/
  DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
