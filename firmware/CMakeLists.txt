cmake_minimum_required(VERSION 3.19)

foreach(var WIFI_SSID WIFI_PASSWORD PIPECAT_SMALLWEBRTC_URL)
  if(NOT DEFINED ENV{${var}})
    message(FATAL_ERROR "Env variable ${var} must be set -- see firmware/.env.example")
  endif()
  # Accept .env values written quoted or bare: a stray surrounding quote (e.g. from
  # an env loaded by a mechanism other than firmware/README.md's documented ones)
  # lands inside the string literal as a user-defined-literal suffix, failing the
  # build far from the cause.
  string(REGEX REPLACE "^[\"'](.*)[\"']$" "\\1" value "$ENV{${var}}")
  if(value STREQUAL "")
    message(FATAL_ERROR "Env variable ${var} must be set -- see firmware/.env.example")
  endif()
  set(${var}_value "${value}")
  add_compile_definitions(${var}="${value}")
endforeach()

# A PIPECAT_SMALLWEBRTC_URL without an explicit port silently dials port 80, where
# nothing listens -- the agent server is on 7860 by default. On device that surfaces
# only as an HTTP connect failure and a reboot loop, so catch it here instead.
if(NOT PIPECAT_SMALLWEBRTC_URL_value MATCHES "^https?://[^/:]+:[0-9]+/.+$")
  message(FATAL_ERROR
    "PIPECAT_SMALLWEBRTC_URL must be http(s)://<host>:<port>/<path> with an explicit "
    "port -- got '${PIPECAT_SMALLWEBRTC_URL_value}'. The agent server listens on 7860 "
    "by default, e.g. http://192.168.1.10:7860/api/offer. See firmware/.env.example.")
endif()

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
