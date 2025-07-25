# SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
#
# SPDX-License-Identifier: Apache-2.0

################################################################################

# Setting operation version of CMake.
cmake_minimum_required(VERSION 3.21)

# Setting C++11 for Apple
if(APPLE)
  set(CMAKE_CXX_STANDARD 11)
endif()

project(SensCord VERSION 1.11.1)
set(SensCord_VERSION_DESCRIPTION "")

message(STATUS "${PROJECT_NAME} ${SensCord_VERSION}")
#message(STATUS "major  : ${SensCord_VERSION_MAJOR}")
#message(STATUS "minor  : ${SensCord_VERSION_MINOR}")
#message(STATUS "patch  : ${SensCord_VERSION_PATCH}")

find_package(Git QUIET)
if(Git_FOUND)
  execute_process(
      COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      OUTPUT_VARIABLE PROJECT_VCS_BRANCH
      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process(
      COMMAND ${GIT_EXECUTABLE} log -1 --pretty=format:%H
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      OUTPUT_VARIABLE PROJECT_VCS_COMMIT
      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process(
      COMMAND ${GIT_EXECUTABLE} log -1 --pretty=format:%cd
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      OUTPUT_VARIABLE PROJECT_VCS_COMMIT_DATE
      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
  message(STATUS "branch : ${PROJECT_VCS_BRANCH}")
  message(STATUS "commit : ${PROJECT_VCS_COMMIT}")
  message(STATUS "date   : ${PROJECT_VCS_COMMIT_DATE}")
endif()

# set CMake Module directories path
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

# Restrict build configuration to Debug and Release
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

################################################################################
#  Common variables
################################################################################

# Set the name of core
set(SSDK_CORE_NAME senscord)

# Set top directory variable
set(SSDK_TOP_DIR ${PROJECT_SOURCE_DIR})

# Module path
set(SSDK_MODULE_DIR ${SSDK_TOP_DIR}/cmake)

# set include directory path
set(SSDK_INCLUDE_DIR ${SSDK_TOP_DIR}/include)

# Setting the output destination directory
set(SSDK_BUILD_OUTPUT_DIR ${CMAKE_BINARY_DIR}/output)

# Output include directory.
set(SSDK_BUILD_OUTPUT_INCLUDE_DIR ${SSDK_BUILD_OUTPUT_DIR}/include)

# Destination directory.
set(SSDK_BINARY_OUT_DIR ${SSDK_BUILD_OUTPUT_DIR}/bin)

# Destination directory.
set(SSDK_TEST_OUT_DIR ${SSDK_BUILD_OUTPUT_DIR}/bin)

# Config destination directory.
set(SSDK_CONFIG_OUT_DIR ${SSDK_BUILD_OUTPUT_DIR}/config)

if (WIN32)
  # Core destination directory.
  set(SSDK_LIBRARY_OUT_DIR ${SSDK_BUILD_OUTPUT_DIR}/bin)
else()
  # Core destination directory.
  set(SSDK_LIBRARY_OUT_DIR ${SSDK_BUILD_OUTPUT_DIR}/lib)
endif()

# Component destination directory.
set(SSDK_COMPONENT_OUT_DIR ${SSDK_LIBRARY_OUT_DIR}/component)

# Allocator estination directory.
set(SSDK_ALLOCATOR_OUT_DIR ${SSDK_LIBRARY_OUT_DIR}/allocator)

# Recorder destination directory.
set(SSDK_RECORDER_OUT_DIR ${SSDK_LIBRARY_OUT_DIR}/recorder)

# Connection destination directory.
set(SSDK_CONNECTION_OUT_DIR ${SSDK_LIBRARY_OUT_DIR}/connection)

# Converter destination directory.
set(SSDK_CONVERTER_OUT_DIR ${SSDK_LIBRARY_OUT_DIR}/converter)

# Component header reference.
set(SSDK_COMPONENT_HEADER_DIR ${SSDK_BUILD_OUTPUT_DIR}/include)

# Header destination directory.
set(SSDK_HEADER_OUT_DIR ${SSDK_COMPONENT_HEADER_DIR}/${SSDK_CORE_NAME})

# Python module output directory.
set(SSDK_PYTHON_OUT_DIR ${SSDK_BUILD_OUTPUT_DIR}/python)

# Utility destination directory
set(SSDK_UTIL_BINARY_OUT_DIR ${SSDK_BINARY_OUT_DIR}/utility)
set(SSDK_UTIL_LIBRARY_OUT_DIR ${SSDK_LIBRARY_OUT_DIR}/utility)
set(SSDK_UTIL_TEST_OUT_DIR ${SSDK_TEST_OUT_DIR}/utility)

################################################################################
# Environmental setting
################################################################################

# set target cpu
set(BUILD_TARGET_PLATFORM "x86")

if (WIN32 AND CMAKE_CL_64)

    set(BUILD_TARGET_PLATFORM "x64")

endif()

################################################################################
#  Options
################################################################################
# Log

option(SENSCORD_LOG_ENABLED         "Enables the senscord log."        OFF)
option(SENSCORD_LOG_OSAL_ENABLED    "Enables the osal log."            OFF)
option(SENSCORD_LOG_TIME_ENABLED    "Adds the system time to the log." OFF)
if(WIN32)
  set(SENSCORD_LOG_TYPE "CONSOLE" CACHE STRING
      "Specifies the log output type. (CONSOLE or FILE)")
else()
  set(SENSCORD_LOG_TYPE "CONSOLE" CACHE STRING
      "Specifies the log output type. (CONSOLE, FILE or SYSLOG)")
endif()

if(SENSCORD_LOG_ENABLED)
  if(${SENSCORD_LOG_TYPE} STREQUAL "CONSOLE")
    set(SENSCORD_LOG_TYPE_CONSOLE ON)
  elseif(${SENSCORD_LOG_TYPE} STREQUAL "FILE")
    set(SENSCORD_LOG_TYPE_FILE ON)
  elseif(${SENSCORD_LOG_TYPE} STREQUAL "SYSLOG" AND UNIX)
    set(SENSCORD_LOG_TYPE_SYSLOG ON)
  else()
    message(FATAL_ERROR
        "Invalid log type \"${SENSCORD_LOG_TYPE}\" has been set")
  endif()
else()
  if(NOT SENSCORD_LOG_OSAL_ENABLED)
    unset(SENSCORD_LOG_TIME_ENABLED CACHE)
  endif()
  unset(SENSCORD_LOG_TYPE CACHE)
endif()

if(SENSCORD_LOG_OSAL_ENABLED)
  set(SENSCORD_OSAL_LOG_LEVEL "senscord::osal::LOG_WARNING" CACHE STRING
      "Specifies the log level. (LOG_ERROR, LOG_WARNING, LOG_INFO or LOG_DEBUG)")
else()
  unset(SENSCORD_OSAL_LOG_LEVEL CACHE)
endif()

################################################################################
# Status

option(SENSCORD_STATUS_MESSAGE_ENABLED  "Enables error messages for Status."    ON)
option(SENSCORD_STATUS_TRACE_ENABLED    "Enables trace information for Status." OFF)

if(SENSCORD_STATUS_TRACE_ENABLED)
  set(SENSCORD_STATUS_MESSAGE_ENABLED ON)
endif()

################################################################################
# Multi-process

option(SENSCORD_SERVER              "Enables the server / client."                  ON)
option(SENSCORD_SERVER_ROS          "Enables the server for SensCord ROS Package."  OFF)
option(SENSCORD_SERVER_WEBSOCKET    "Enables the WebSocket server."                 OFF)
option(SENSCORD_SERVER_SETTING      "Enables the server setting."                   ON)
option(SENSCORD_SERVER_SEARCH_SSDP  "Enables the server search ssdp."               OFF)
option(SENSCORD_SERVER_SEARCH_UCOM  "Enables the server search ucom."               OFF)

if(DEFINED SENSCORD_SERVER_LEGACY)  ### deprecated option
  set(SENSCORD_SERVER_ROS ${SENSCORD_SERVER_LEGACY} CACHE BOOL "" FORCE)
  unset(SENSCORD_SERVER_LEGACY CACHE)
endif()

if(DEFINED WEBSOCKET_SERVER_ENABLE)  ### deprecated option
  set(SENSCORD_SERVER_WEBSOCKET ${WEBSOCKET_SERVER_ENABLE} CACHE BOOL "" FORCE)
  unset(WEBSOCKET_SERVER_ENABLE CACHE)
endif()

if(DEFINED FUNCTION_SERVER_SEARCH_SSDP_ENABLE)  ### deprecated option
  set(SENSCORD_SERVER_SEARCH_SSDP ${FUNCTION_SERVER_SEARCH_SSDP_ENABLE} CACHE BOOL "" FORCE)
  unset(FUNCTION_SERVER_SEARCH_SSDP_ENABLE CACHE)
endif()

if(DEFINED FUNCTION_SERVER_SEARCH_UCOM_ENABLE)  ### deprecated option
  set(SENSCORD_SERVER_SEARCH_UCOM ${FUNCTION_SERVER_SEARCH_UCOM_ENABLE} CACHE BOOL "" FORCE)
  unset(FUNCTION_SERVER_SEARCH_UCOM_ENABLE CACHE)
endif()

if(ANDROID OR APPLE)
  unset(SENSCORD_SERVER_SEARCH_SSDP CACHE)
  unset(SENSCORD_SERVER_SEARCH_UCOM CACHE)
endif()

if(SENSCORD_SERVER_SEARCH_SSDP OR SENSCORD_SERVER_SEARCH_UCOM)
  set(SENSCORD_SERVER_SETTING ON)
endif()

if(SENSCORD_SERVER_ROS OR SENSCORD_SERVER_WEBSOCKET OR SENSCORD_SERVER_SETTING)
  set(SENSCORD_SERVER ON)
endif()

################################################################################
# Recorder / Player

option(SENSCORD_RECORDER_RAW    "Enables the raw recorder."             ON)
option(SENSCORD_RECORDER_SKV    "Enables the skv recorder."             OFF)
option(SENSCORD_RECORDER_LOADER "Enables the external recorder loader." ON)
option(SENSCORD_RECORDER_BIN    "Enables the external bin recorder."    ON)
option(SENSCORD_RECORDER_PNM    "Enables the external pnm recorder."    ON)

option(SENSCORD_PLAYER_RAW      "Enables the raw player."               ON)
option(SENSCORD_PLAYER_SKV      "Enables the skv player."               OFF)

if(DEFINED FUNCTION_SKV_ENABLE)  ### deprecated option
  set(SENSCORD_RECORDER_SKV ${FUNCTION_SKV_ENABLE} CACHE BOOL "" FORCE)
  set(SENSCORD_PLAYER_SKV ${FUNCTION_SKV_ENABLE} CACHE BOOL "" FORCE)
  unset(FUNCTION_SKV_ENABLE CACHE)
endif()

if(ANDROID OR APPLE)
  unset(SENSCORD_RECORDER_SKV CACHE)
  unset(SENSCORD_PLAYER_SKV CACHE)
endif()

if(SENSCORD_RECORDER_BIN OR SENSCORD_RECORDER_PNM)
  set(SENSCORD_RECORDER_LOADER ON)
endif()

if(SENSCORD_RECORDER_RAW OR SENSCORD_RECORDER_SKV OR SENSCORD_RECORDER_LOADER)
  set(SENSCORD_RECORDER ON)
endif()

if(SENSCORD_PLAYER_RAW OR SENSCORD_PLAYER_SKV)
  set(SENSCORD_PLAYER ON)
endif()

################################################################################
# API

option(SENSCORD_API_C       "Enables the C-Lang API."   OFF)
option(SENSCORD_API_PYTHON  "Enables the Python API."   OFF)
option(SENSCORD_API_CSHARP  "Enables the C# API."       OFF)
option(SENSCORD_API_JAVA    "Enables the Java API."     OFF)
option(SENSCORD_API_WASM    "Enables the WASM API."     OFF)

if(DEFINED C_API_ENABLE)  ### deprecated option
  set(SENSCORD_API_C ${C_API_ENABLE} CACHE BOOL "" FORCE)
  unset(C_API_ENABLE CACHE)
endif()

if(DEFINED PYTHON_API_ENABLE)  ### deprecated option
  set(SENSCORD_API_PYTHON ${PYTHON_API_ENABLE} CACHE BOOL "" FORCE)
  unset(PYTHON_API_ENABLE CACHE)
endif()

# TODO: WASM API support for Windows
if(WIN32 OR APPLE)
  unset(SENSCORD_API_WASM CACHE)
endif()

if(SENSCORD_API_PYTHON OR SENSCORD_API_CSHARP OR SENSCORD_API_JAVA OR SENSCORD_API_WASM)
  set(SENSCORD_API_C ON)
endif()

################################################################################
# Core functions

option(SENSCORD_STREAM_VERSION          "Enables the stream version."   ON)
option(SENSCORD_STREAM_EVENT_ARGUMENT   "Enables the event argument."   ON)

################################################################################
# Allocator

option(SENSCORD_ALLOCATOR_SHARED_MEMORY "Enables the shared memory allocator."  ON)
option(SENSCORD_ALLOCATOR_SAMPLE        "Enables the allocator sample."         OFF)

if(DEFINED ALLOCATOR_SAMPLE_ENABLE)  ### deprecated option
  set(SENSCORD_ALLOCATOR_SAMPLE ${ALLOCATOR_SAMPLE_ENABLE} CACHE BOOL "" FORCE)
  unset(ALLOCATOR_SAMPLE_ENABLE CACHE)
endif()

if(ANDROID OR APPLE)
  unset(SENSCORD_ALLOCATOR_SHARED_MEMORY CACHE)
endif()

################################################################################
# Components

option(SENSCORD_COMPONENT_PSEUDO    "Enables the pseudo component." OFF)
option(SENSCORD_COMPONENT_V4L2      "Enables the v4l2 component."   OFF)
option(SENSCORD_COMPONENT_OPENCV    "Enables the opencv component." OFF)
option(SENSCORD_COMPONENT_AUDIO_PCM "Enables the pcm audio component."  OFF)

if(DEFINED COMPONENT_IMAGE_PSEUDO_ENABLE)  ### deprecated option
  set(SENSCORD_COMPONENT_PSEUDO ${COMPONENT_IMAGE_PSEUDO_ENABLE} CACHE BOOL "" FORCE)
  unset(COMPONENT_IMAGE_PSEUDO_ENABLE CACHE)
endif()

if(DEFINED COMPONENT_IMAGE_V4L2_ENABLE)  ### deprecated option
  set(SENSCORD_COMPONENT_V4L2 ${COMPONENT_IMAGE_V4L2_ENABLE} CACHE BOOL "" FORCE)
  unset(COMPONENT_IMAGE_V4L2_ENABLE CACHE)
endif()

if(WIN32 OR APPLE)
  unset(SENSCORD_COMPONENT_V4L2 CACHE)
  unset(SENSCORD_COMPONENT_AUDIO_PCM CACHE)
endif()

################################################################################
# Test

option(SENSCORD_TEST_CORE   "Enables senscord test."    OFF)
option(SENSCORD_TEST_OSAL   "Enables osal test."        OFF)

if(DEFINED SENSCORD_TEST_CODE_ENABLE)  ### deprecated option
  set(SENSCORD_TEST_CORE ${SENSCORD_TEST_CODE_ENABLE} CACHE BOOL "" FORCE)
  unset(SENSCORD_TEST_CODE_ENABLE CACHE)
endif()

if(DEFINED OSAL_TEST_CODE_ENABLE)  ### deprecated option
  set(SENSCORD_TEST_OSAL ${OSAL_TEST_CODE_ENABLE} CACHE BOOL "" FORCE)
  unset(OSAL_TEST_CODE_ENABLE CACHE)
endif()

## Enable test function.
if(SENSCORD_TEST_CORE OR SENSCORD_TEST_OSAL)
  set(SENSCORD_TEST_LARGE_FILE OFF CACHE BOOL "")
  mark_as_advanced(SENSCORD_TEST_LARGE_FILE)

  enable_testing()

  if(UNIX)
    # Memory check test
    #  [Command example]
    #    cmake .. -DCMAKE_BUILD_TYPE=Debug -DSENSCORD_TEST_CORE=ON && make
    #    ctest -T memcheck [options]
    #    ctest -T memcheck -V -R <tests-regex>
    #    ctest -T memcheck -E "performance|multi_process_and_thread|py_test"
    set(MEMORYCHECK_COMMAND_OPTIONS "--fullpath-after= --error-limit=no --leak-check=full --show-leak-kinds=definite,possible --xml=yes --xml-file=${CMAKE_BINARY_DIR}/Testing/valgrind_result_%p.xml --num-callers=100 --show-below-main=yes --default-suppressions=no")
    find_program(CTEST_MEMORYCHECK_COMMAND NAMES valgrind)
    include(Dart)
  endif()
else()
  unset(SENSCORD_TEST_LARGE_FILE CACHE)
endif()

################################################################################
# Serialize

option(SENSCORD_SERIALIZE "Enables the serialization function." ON)

if(SENSCORD_SERVER)
  list(APPEND SENSCORD_SERIALIZE_DEPENDS "SENSCORD_SERVER")
endif()
if(SENSCORD_RECORDER)
  list(APPEND SENSCORD_SERIALIZE_DEPENDS "SENSCORD_RECORDER")
endif()
if(SENSCORD_PLAYER)
  list(APPEND SENSCORD_SERIALIZE_DEPENDS "SENSCORD_PLAYER")
endif()
if(SENSCORD_API_PYTHON)
  list(APPEND SENSCORD_SERIALIZE_DEPENDS "SENSCORD_API_PYTHON")
endif()
if(SENSCORD_API_CSHARP)
  list(APPEND SENSCORD_SERIALIZE_DEPENDS "SENSCORD_API_CSHARP")
endif()
if(SENSCORD_STREAM_EVENT_ARGUMENT)
  list(APPEND SENSCORD_SERIALIZE_DEPENDS "SENSCORD_STREAM_EVENT_ARGUMENT")
endif()

if(SENSCORD_SERIALIZE_DEPENDS)
  set(SENSCORD_SERIALIZE ON)
endif()

if(SENSCORD_SERIALIZE)
  ## use bundled msgpack
  option(SENSCORD_USE_BUNDLED_MSGPACK "Enables the bundled msgpack." ON)
  if(DEFINED BUNDLED_MSGPACK_ENABLE)  ### deprecated option
    set(SENSCORD_USE_BUNDLED_MSGPACK ${BUNDLED_MSGPACK_ENABLE} CACHE BOOL "" FORCE)
    unset(BUNDLED_MSGPACK_ENABLE CACHE)
  endif()
else()
  unset(SENSCORD_USE_BUNDLED_MSGPACK CACHE)
endif()

################################################################################
# Utility

## Synchronizer
option(SENSCORD_SYNCHRONIZER "Enable build Synchronizer utility." OFF)
option(SENSCORD_SYNCHRONIZER_SAMPLE "Enable build Synchronizer sample codes." OFF)
set(SENSCORD_SYNCHRONIZER_TEST_MODE "Disable" CACHE STRING
    "Specify Synchronizer test mode: Disable/Normal/Manual/Coverage/Valgrind")

if(DEFINED SYNCHRONIZER_ENABLE)  ### deprecated option
  set(SENSCORD_SYNCHRONIZER ${SYNCHRONIZER_ENABLE} CACHE BOOL "" FORCE)
  unset(SYNCHRONIZER_ENABLE CACHE)
endif()

if(DEFINED SYNCHRONIZER_SAMPLE_ENABLE)  ### deprecated option
  set(SENSCORD_SYNCHRONIZER_SAMPLE ${SYNCHRONIZER_SAMPLE_ENABLE} CACHE BOOL "" FORCE)
  unset(SYNCHRONIZER_SAMPLE_ENABLE CACHE)
endif()

if(DEFINED SYNCHRONIZER_TEST_MODE)  ### deprecated option
  set(SENSCORD_SYNCHRONIZER_TEST_MODE ${SYNCHRONIZER_TEST_MODE} CACHE STRING "" FORCE)
  unset(SYNCHRONIZER_TEST_MODE CACHE)
endif()

if(SENSCORD_SYNCHRONIZER_SAMPLE OR NOT SENSCORD_SYNCHRONIZER_TEST_MODE STREQUAL "Disable")
  set(SENSCORD_SYNCHRONIZER ON)
endif()

if(ANDROID OR APPLE)
  unset(SENSCORD_SYNCHRONIZER CACHE)
  unset(SENSCORD_SYNCHRONIZER_SAMPLE CACHE)
  unset(SENSCORD_SYNCHRONIZER_TEST_MODE CACHE)
endif()

################################################################################
# Others

## Build sample
option(SENSCORD_SAMPLE "Enables the sample source."
       OFF)
if(DEFINED ENABLE_BUILD_SAMPLE)  ### deprecated option
  set(SENSCORD_SAMPLE ${ENABLE_BUILD_SAMPLE} CACHE BOOL "" FORCE)
  unset(ENABLE_BUILD_SAMPLE CACHE)
endif()

if(IOS)
  unset(SENSCORD_SAMPLE CACHE)
endif()

if(SENSCORD_SAMPLE)
  set(SENSCORD_COMPONENT_PSEUDO ON)
endif()

## Install senscord configuration file
option(SENSCORD_INSTALL_CONFIG "Install senscord configuration file."
       OFF)
if(DEFINED INSTALL_SENSCORD_CONFIG)  ### deprecated option
  set(SENSCORD_INSTALL_CONFIG ${INSTALL_SENSCORD_CONFIG} CACHE BOOL "" FORCE)
  unset(INSTALL_SENSCORD_CONFIG CACHE)
endif()

################################################################################

message(STATUS "senscord build options...")

message(STATUS "Logger")
message(STATUS "* SENSCORD_LOG_ENABLED            -> ${SENSCORD_LOG_ENABLED}")
message(STATUS "* SENSCORD_LOG_OSAL_ENABLED       -> ${SENSCORD_LOG_OSAL_ENABLED}")
if(DEFINED SENSCORD_LOG_TYPE)
  message(STATUS "** SENSCORD_LOG_TYPE              -> ${SENSCORD_LOG_TYPE}")
endif()
if(DEFINED SENSCORD_LOG_TIME_ENABLED)
  message(STATUS "** SENSCORD_LOG_TIME_ENABLED      -> ${SENSCORD_LOG_TIME_ENABLED}")
endif()

message(STATUS "Status")
message(STATUS "* SENSCORD_STATUS_MESSAGE_ENABLED -> ${SENSCORD_STATUS_MESSAGE_ENABLED}")
message(STATUS "** SENSCORD_STATUS_TRACE_ENABLED  -> ${SENSCORD_STATUS_TRACE_ENABLED}")

message(STATUS "Multi-process")
message(STATUS "* SENSCORD_SERVER                 -> ${SENSCORD_SERVER}")
message(STATUS "** SENSCORD_SERVER_ROS            -> ${SENSCORD_SERVER_ROS}")
message(STATUS "** SENSCORD_SERVER_WEBSOCKET      -> ${SENSCORD_SERVER_WEBSOCKET}")
message(STATUS "** SENSCORD_SERVER_SETTING        -> ${SENSCORD_SERVER_SETTING}")
if(DEFINED SENSCORD_SERVER_SEARCH_SSDP)
  message(STATUS "*** SENSCORD_SERVER_SEARCH_SSDP   -> ${SENSCORD_SERVER_SEARCH_SSDP}")
endif()
if(DEFINED SENSCORD_SERVER_SEARCH_UCOM)
  message(STATUS "*** SENSCORD_SERVER_SEARCH_UCOM   -> ${SENSCORD_SERVER_SEARCH_UCOM}")
endif()

message(STATUS "Recorder")
message(STATUS "* SENSCORD_RECORDER_RAW           -> ${SENSCORD_RECORDER_RAW}")
if(DEFINED SENSCORD_RECORDER_SKV)
  message(STATUS "* SENSCORD_RECORDER_SKV           -> ${SENSCORD_RECORDER_SKV}")
endif()
message(STATUS "* SENSCORD_RECORDER_LOADER        -> ${SENSCORD_RECORDER_LOADER}")
message(STATUS "** SENSCORD_RECORDER_BIN          -> ${SENSCORD_RECORDER_BIN}")
message(STATUS "** SENSCORD_RECORDER_PNM          -> ${SENSCORD_RECORDER_PNM}")

message(STATUS "Player")
message(STATUS "* SENSCORD_PLAYER_RAW             -> ${SENSCORD_PLAYER_RAW}")
if(DEFINED SENSCORD_PLAYER_SKV)
  message(STATUS "* SENSCORD_PLAYER_SKV             -> ${SENSCORD_PLAYER_SKV}")
endif()

message(STATUS "API")
message(STATUS "* SENSCORD_API_C                  -> ${SENSCORD_API_C}")
message(STATUS "** SENSCORD_API_PYTHON            -> ${SENSCORD_API_PYTHON}")
message(STATUS "** SENSCORD_API_CSHARP            -> ${SENSCORD_API_CSHARP}")
message(STATUS "** SENSCORD_API_JAVA              -> ${SENSCORD_API_JAVA}")
if(DEFINED SENSCORD_API_WASM)
  message(STATUS "** SENSCORD_API_WASM              -> ${SENSCORD_API_WASM}")
endif()

message(STATUS "Core functions")
message(STATUS "* SENSCORD_STREAM_VERSION         -> ${SENSCORD_STREAM_VERSION}")
message(STATUS "* SENSCORD_STREAM_EVENT_ARGUMENT  -> ${SENSCORD_STREAM_EVENT_ARGUMENT}")

message(STATUS "Allocator")
if(DEFINED SENSCORD_ALLOCATOR_SHARED_MEMORY)
  message(STATUS "* SENSCORD_ALLOCATOR_SHARED_MEMORY -> ${SENSCORD_ALLOCATOR_SHARED_MEMORY}")
endif()
message(STATUS "* SENSCORD_ALLOCATOR_SAMPLE       -> ${SENSCORD_ALLOCATOR_SAMPLE}")

message(STATUS "Component")
message(STATUS "* SENSCORD_COMPONENT_PSEUDO       -> ${SENSCORD_COMPONENT_PSEUDO}")
if(DEFINED SENSCORD_COMPONENT_V4L2)
  message(STATUS "* SENSCORD_COMPONENT_V4L2         -> ${SENSCORD_COMPONENT_V4L2}")
endif()
message(STATUS "* SENSCORD_COMPONENT_OPENCV       -> ${SENSCORD_COMPONENT_OPENCV}")
if(DEFINED SENSCORD_COMPONENT_AUDIO_PCM)
  message(STATUS "* SENSCORD_COMPONENT_AUDIO_PCM    -> ${SENSCORD_COMPONENT_AUDIO_PCM}")
endif()

message(STATUS "Test")
message(STATUS "* SENSCORD_TEST_CORE              -> ${SENSCORD_TEST_CORE}")
message(STATUS "* SENSCORD_TEST_OSAL              -> ${SENSCORD_TEST_OSAL}")

if(DEFINED SENSCORD_SYNCHRONIZER)
message(STATUS "Utility")
  message(STATUS "* SENSCORD_SYNCHRONIZER           -> ${SENSCORD_SYNCHRONIZER}")
  message(STATUS "* SENSCORD_SYNCHRONIZER_SAMPLE    -> ${SENSCORD_SYNCHRONIZER_SAMPLE}")
  message(STATUS "* SENSCORD_SYNCHRONIZER_TEST_MODE -> ${SENSCORD_SYNCHRONIZER_TEST_MODE}")
endif()

message(STATUS "Others")
message(STATUS "* SENSCORD_SERIALIZE              -> ${SENSCORD_SERIALIZE}")
if(SENSCORD_SERIALIZE_DEPENDS)
  message(STATUS "    [${SENSCORD_SERIALIZE_DEPENDS}]")
endif()
if(DEFINED SENSCORD_USE_BUNDLED_MSGPACK)
  message(STATUS "** SENSCORD_USE_BUNDLED_MSGPACK   -> ${SENSCORD_USE_BUNDLED_MSGPACK}")
endif()
message(STATUS "* SENSCORD_SAMPLE                 -> ${SENSCORD_SAMPLE}")
message(STATUS "* SENSCORD_INSTALL_CONFIG         -> ${SENSCORD_INSTALL_CONFIG}")

################################################################################
#  Install
################################################################################

# set binary file install path
set(SSDK_BIN_INSTALL_DIR bin)

# set library file install path
set(SSDK_LIBRARY_INSTALL_DIR lib)

# set cmake module file install path
set(SSDK_CMAKE_INSTALL_DIR ${SSDK_LIBRARY_INSTALL_DIR}/cmake/${SSDK_CORE_NAME})

# set include file install path
set(SSDK_INCLUDE_INSTALL_DIR include)

# set include/senscord install path
set(SSDK_INCLUDE_SENSCORD_INSTALL_DIR
    ${SSDK_INCLUDE_INSTALL_DIR}/${SSDK_CORE_NAME})

# set include/senscord/develop install path
set(SSDK_INCLUDE_SENSCORD_DEVELOP_INSTALL_DIR
    ${SSDK_INCLUDE_SENSCORD_INSTALL_DIR}/develop)

if(SENSCORD_API_C)
  # Set include/senscord/c_api install path
  set(SSDK_INCLUDE_SENSCORD_C_API_INSTALL_DIR
      ${SSDK_INCLUDE_SENSCORD_INSTALL_DIR}/c_api)
endif()

if (WIN32)
  # set documet file install path
  set(SSDK_DOCUMENT_INSTALL_DIR doc)

  # set library file install path
  set(SSDK_CONIFIG_INSTALL_DIR config)

  # set component library file install path
  set(SSDK_COMPONENT_INSTALL_DIR ${SSDK_BIN_INSTALL_DIR}/component)

  # set allocator library file install path
  set(SSDK_ALLOCATOR_INSTALL_DIR ${SSDK_BIN_INSTALL_DIR}/allocator)

  # set recorder library file install path
  set(SSDK_RECORDER_INSTALL_DIR ${SSDK_BIN_INSTALL_DIR}/recorder)

  # set connection library file install path
  set(SSDK_CONNECTION_INSTALL_DIR ${SSDK_BIN_INSTALL_DIR}/connection)

  # set osal library file install path
  set(SSDK_OSAL_INSTALL_DIR ${SSDK_BIN_INSTALL_DIR})

  # set static library file install path
  set(SSDK_STATIC_LIBRARY_INSTALL_DIR ${SSDK_LIBRARY_INSTALL_DIR})

  # set python module install path
  string(REPLACE "C:/" "C:\\\\" SSDK_PYTHON_INSTALL_DIR ${CMAKE_INSTALL_PREFIX})
else()

  # specify installation directory
  set(SSDK_SENSCORD_DATA_INSTALL_DIR share/${SSDK_CORE_NAME})

  # set documet file install path
  set(SSDK_DOCUMENT_INSTALL_DIR ${SSDK_SENSCORD_DATA_INSTALL_DIR}/doc)

  # set library file install path
  set(SSDK_CONIFIG_INSTALL_DIR ${SSDK_SENSCORD_DATA_INSTALL_DIR}/config)

  # set component library file install path
  set(SSDK_COMPONENT_INSTALL_DIR
      ${SSDK_LIBRARY_INSTALL_DIR}/${SSDK_CORE_NAME}/component)

  # set allocator library file install path
  set(SSDK_ALLOCATOR_INSTALL_DIR
      ${SSDK_LIBRARY_INSTALL_DIR}/${SSDK_CORE_NAME}/allocator)

  # set recorder library file install path
  set(SSDK_RECORDER_INSTALL_DIR
      ${SSDK_LIBRARY_INSTALL_DIR}/${SSDK_CORE_NAME}/recorder)

  # set connection library file install path
  set(SSDK_CONNECTION_INSTALL_DIR
      ${SSDK_LIBRARY_INSTALL_DIR}/${SSDK_CORE_NAME}/connection)

  # set osal library file install path
  set(SSDK_OSAL_INSTALL_DIR ${SSDK_LIBRARY_INSTALL_DIR})

  # set static library file install path
  set(SSDK_STATIC_LIBRARY_INSTALL_DIR
      ${SSDK_LIBRARY_INSTALL_DIR}/${SSDK_CORE_NAME})

  # set python module install path
  set(SSDK_PYTHON_INSTALL_DIR ${CMAKE_INSTALL_PREFIX})
endif()

# set utility install path
set(UTILITY_BIN_INSTALL_DIR "bin/")
if(WIN32)
  set(UTILITY_SHARED_LIB_INSTALL_DIR "bin/utility/")
  set(UTILITY_WIN_LIB_INSTALL_DIR "lib/")
else()
  set(UTILITY_SHARED_LIB_INSTALL_DIR "lib/senscord/utility/")
endif()

# Set the name of export
set(SSDK_EXPORT_NAME ${SSDK_CORE_NAME}_export)

# Set the name of cmake configure file
set(SSDK_EXPORT_CONFIG ${SSDK_CORE_NAME}-config.cmake)

################################################################################
#  Custom targets
################################################################################
# Clean output directory.
add_custom_target(clean_output
                  COMMAND ${CMAKE_COMMAND}
                          -E remove_directory ${SSDK_BUILD_OUTPUT_DIR})

# Target to enabled all options.
add_custom_target(option_all_enabled
                  COMMAND ${CMAKE_COMMAND}
                          -DCMAKELISTS_DIR="${SSDK_TOP_DIR}"
                          -DOPTION_SWITCH=ON
                          -P ${SSDK_MODULE_DIR}/SwitchAllOptions.cmake)

# Target to diabled all options.
add_custom_target(option_all_disabled
                  COMMAND ${CMAKE_COMMAND}
                          -DCMAKELISTS_DIR="${SSDK_TOP_DIR}"
                          -DOPTION_SWITCH=OFF
                          -P ${SSDK_MODULE_DIR}/SwitchAllOptions.cmake)

# Target to enabled all tests.
add_custom_target(test_all_enabled
                  COMMAND ${CMAKE_COMMAND}
                          -DCMAKELISTS_DIR="${SSDK_TOP_DIR}"
                          -DOPTION_SWITCH=ON
                          -P ${SSDK_MODULE_DIR}/SwitchAllTest.cmake)

# Target to diabled all tests.
add_custom_target(test_all_disabled
                  COMMAND ${CMAKE_COMMAND}
                          -DCMAKELISTS_DIR="${SSDK_TOP_DIR}"
                          -DOPTION_SWITCH=OFF
                          -P ${SSDK_MODULE_DIR}/SwitchAllTest.cmake)

if(UNIX)
  add_custom_target(clean_all
                    COMMAND ${CMAKE_BUILD_TOOL} clean
                    COMMAND ${CMAKE_BUILD_TOOL} clean_output)
endif()

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Coverage")
  add_custom_target(clean_gcov
                    COMMAND rm -f `find -name \"*.gcno\"`
                    COMMAND rm -f `find -name \"*.gcda\"`
                    COMMAND rm -f `find -name \"*.gcov\"`)
  add_dependencies(clean_all clean_gcov)
endif()

# Running cpplint
add_custom_target(cpplint-all)

################################################################################
#  Setting for IDE
################################################################################

if(WIN32)
  # Set target folder name
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
  set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "cmake_common")
  set_target_properties(clean_output PROPERTIES FOLDER "custom_targets")
  set_target_properties(option_all_enabled PROPERTIES FOLDER "custom_targets")
  set_target_properties(option_all_disabled PROPERTIES FOLDER "custom_targets")
endif()

################################################################################
#  Build senscord lib
################################################################################

configure_file(
    ${SSDK_INCLUDE_DIR}/senscord/config.h.in
    ${SSDK_BUILD_OUTPUT_INCLUDE_DIR}/senscord/config.h)
configure_file(
    ${SSDK_INCLUDE_DIR}/senscord/logger_config.h.in
    ${SSDK_BUILD_OUTPUT_INCLUDE_DIR}/senscord/logger_config.h)

if(WIN32)
  set(TEMPLATE_WINDOWS_APP_VERSION "${PROJECT_SOURCE_DIR}/template/windows/app_version.rc.in")
  set(TEMPLATE_WINDOWS_DLL_VERSION "${PROJECT_SOURCE_DIR}/template/windows/dll_version.rc.in")
endif()

# Setting common compile options for the entire build environment.
if (WIN32)
  # Suppress visual studio warning about character code of file.
  add_definitions(/wd4819)

  # exclude C4100(cause:use override)
  add_definitions(/wd4100)

  # exclude C4456(cause:use other scope)
  add_definitions(/wd4456)

  set(SSDK_COMPILE_OPTIONS /w14946)
else()
  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(SSDK_COMPILE_OPTIONS
        -Wall -Wextra -Wconversion -Wpedantic -Wshadow
        -Wno-unused-parameter
        -Wno-sign-conversion -Wno-overloaded-virtual
        -Wno-c99-extensions -Wno-variadic-macros)
  else()
    set(SSDK_COMPILE_OPTIONS
        -Wall -Wextra -Wconversion -Wpedantic -Wshadow
        -Wno-unused-parameter)
  endif()
endif()

# Enable project specific utility.
include(SensCordUtils)

# build warning level
set(WARNING_LEVEL_CORE "warning_level_core")
set(WARNING_LEVEL_TEST "warning_level_test")
set_build_warning_level(${WARNING_LEVEL_CORE})

if(APPLE)
  # rename to bundle for Unity on macOS
  set(CMAKE_SHARED_LIBRARY_PREFIX "")
  set(CMAKE_SHARED_LIBRARY_SUFFIX ".bundle")
endif()

# Build thirdparty.
add_subdirectory(thirdparty)

# Build senscord library.
add_subdirectory(lib)

# Install .cmake files
install(EXPORT ${SSDK_EXPORT_NAME}
        FILE ${SSDK_EXPORT_CONFIG}
        DESTINATION ${SSDK_CMAKE_INSTALL_DIR})

# Install license
install(FILES LICENSE DESTINATION .)

################################################################################
#  Build application
################################################################################
if(NOT IOS)
  add_subdirectory(application)
endif()

################################################################################
#  Build sample
################################################################################

if(SENSCORD_SAMPLE)
  message(STATUS "Build sample application.")
  add_subdirectory(sample)
endif()

################################################################################
#  Build api
################################################################################

if(SENSCORD_API_PYTHON)
  add_subdirectory(python)
endif()
if(SENSCORD_API_JAVA)
  add_subdirectory(java)
endif()

################################################################################
#  Coverage
################################################################################
if (UNIX)
  # Add coverage build types
  set(CMAKE_CXX_FLAGS_COVERAGE
      "${GCC_DEBUG_FLAGS} --coverage -O0"
      CACHE STRING "Flags used by the C++ compiler during coverage builds."
      FORCE)
  set(CMAKE_C_FLAGS_COVERAGE
      "${GCC_DEBUG_FLAGS} --coverage -O0"
      CACHE STRING "Flags used by the C compiler during coverage builds."
      FORCE)
  set(CMAKE_EXE_LINKER_FLAGS_COVERAGE
      ""
      CACHE STRING "Flags used for linking binaries during coverage builds."
      FORCE)
  set(CMAKE_SHARED_LINKER_FLAGS_COVERAGE
      ""
      CACHE STRING "Flags used by the shared libraries linker during coverage builds."
      FORCE)
  mark_as_advanced(
      CMAKE_CXX_FLAGS_COVERAGE
      CMAKE_C_FLAGS_COVERAGE
      CMAKE_EXE_LINKER_FLAGS_COVERAGE
      CMAKE_SHARED_LINKER_FLAGS_COVERAGE)
  # Update help
  set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}" CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel Coverage."
      FORCE)
  # Add gcov linkage
  if ("${CMAKE_BUILD_TYPE}" STREQUAL "Coverage")
    set(COMMON_GCOV_LIBRARY PUBLIC gcov)
  endif()
endif()

################################################################################
#  Build utilities
################################################################################

if((NOT SENSCORD_SYNCHRONIZER_TEST_MODE STREQUAL "Disable"))
  # enable_testing() must be called in this directory.
  enable_testing()

  # # setup gcov
  # set(DEBUG_OPTION_GCOV ON)
  # enable_gocv_coverage()

  if(SENSCORD_SYNCHRONIZER_TEST_MODE STREQUAL "Valgrind")
    # Include Dart for ctest with valgrind.
    include(Dart)
  endif()
endif()

add_subdirectory(utility)

