cmake_minimum_required(VERSION 3.19)

foreach(var WIFI_SSID WIFI_PASSWORD PIPECAT_SMALLWEBRTC_URL)
  if(NOT DEFINED ENV{${var}})
    message(FATAL_ERROR "Env variable ${var} must be set -- see firmware/.env.example")
  endif()
  # Accept .env values written quoted or bare: the documented bash loader strips
  # surrounding quotes and fish's cannot, and a stray quote lands inside the string
  # literal as a user-defined-literal suffix, failing the build far from the cause.
  string(REGEX REPLACE "^[\"'](.*)[\"']$" "\\1" value "$ENV{${var}}")
  if(value STREQUAL "")
    message(FATAL_ERROR "Env variable ${var} must be set -- see firmware/.env.example")
  endif()
  add_compile_definitions(${var}="${value}")
endforeach()

if(DEFINED ENV{LOG_DATACHANNEL_MESSAGES})
  add_compile_definitions(LOG_DATACHANNEL_MESSAGES="1")
endif()

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
