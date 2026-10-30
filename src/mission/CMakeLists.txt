cmake_minimum_required(VERSION 3.8)
project(mission)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(mavros_msgs REQUIRED)
find_package(interfaces REQUIRED)

# TODO: add task1 and task2 node executables

ament_package()
