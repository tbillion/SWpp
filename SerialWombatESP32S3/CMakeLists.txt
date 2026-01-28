# Serial Wombat ESP32-S3-N16R8 Port
# ESP-IDF CMake Project

cmake_minimum_required(VERSION 3.16)

# Project information
set(PROJECT_NAME "SerialWombat_ESP32S3")
set(PROJECT_VER "SW18AB-v2.1.0-esp32s3")

# Include ESP-IDF build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Define the project
project(${PROJECT_NAME})
