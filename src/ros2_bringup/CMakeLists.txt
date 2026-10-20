cmake_minimum_required(VERSION 3.8)
project(ros2_bringup)

# Find dependencies
find_package(ament_cmake REQUIRED)
# Uncomment the following section in order to fill in
# further dependencies manually.
# find_package(<dependency> REQUIRED)

# Install launch files
install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME})

ament_package()
