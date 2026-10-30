cmake_minimum_required(VERSION 3.8)
project(mission)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(mavros_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(interfaces REQUIRED)

add_executable(task1 src/task1.cpp)
target_link_libraries(task1
  rclcpp::rclcpp ${geometry_msgs_TARGETS} ${mavros_msgs_TARGETS} ${sensor_msgs_TARGETS})

# TODO: add task2 node executable

install(TARGETS task1 DESTINATION lib/${PROJECT_NAME})

ament_package()
