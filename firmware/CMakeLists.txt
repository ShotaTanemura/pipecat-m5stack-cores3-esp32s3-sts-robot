cmake_minimum_required(VERSION 3.19)

if(NOT DEFINED ENV{WIFI_SSID} OR NOT DEFINED ENV{WIFI_PASSWORD})
  message(FATAL_ERROR "Env variables WIFI_SSID and WIFI_PASSWORD must be set")
endif()

add_compile_definitions(WIFI_SSID="$ENV{WIFI_SSID}")
add_compile_definitions(WIFI_PASSWORD="$ENV{WIFI_PASSWORD}")

if(NOT DEFINED ENV{PIPECAT_SMALLWEBRTC_URL})
  message(FATAL_ERROR "Env variable PIPECAT_SMALLWEBRTC_URL must be set")
endif()

if(DEFINED ENV{LOG_DATACHANNEL_MESSAGES})
  add_compile_definitions(LOG_DATACHANNEL_MESSAGES="1")
endif()

add_compile_definitions(PIPECAT_SMALLWEBRTC_URL="$ENV{PIPECAT_SMALLWEBRTC_URL}")

# pipecat-esp32's esp32-m5stack-cores3 project has no components of its own -- it
# builds against the shared WebRTC/SRTP/Opus components under the esp32-s3-box-3
# variant in the same upstream repo. See docs/adr/0007.
set(PIPECAT_ESP32_DIR "${CMAKE_CURRENT_LIST_DIR}/deps/pipecat-esp32/esp32-s3-box-3")
set(EXTRA_COMPONENT_DIRS
    "${PIPECAT_ESP32_DIR}/components/srtp"
    "${PIPECAT_ESP32_DIR}/components/peer"
    "${PIPECAT_ESP32_DIR}/components/esp-libopus")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(pipecat_robot)
