# SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
#
# SPDX-License-Identifier: Apache-2.0


################################################################################

################################################################################

project(senscord_osal VERSION ${SensCord_VERSION})

# target name
set(TARGET_NAME ${PROJECT_NAME})

set_ssdk_runtime_output_dir(${SSDK_LIBRARY_OUT_DIR})

set(LIB_OSAL_SOURCES
    ${PROJECT_SOURCE_DIR}/osal_cond.cpp
    ${PROJECT_SOURCE_DIR}/osal_directory.cpp
    ${PROJECT_SOURCE_DIR}/osal_stdio.cpp
    ${PROJECT_SOURCE_DIR}/osal_string.cpp
    ${PROJECT_SOURCE_DIR}/osal_mem.cpp
    ${PROJECT_SOURCE_DIR}/osal_file.cpp
    ${PROJECT_SOURCE_DIR}/osal_mutex.cpp
    ${PROJECT_SOURCE_DIR}/osal_dlloader.cpp
    ${PROJECT_SOURCE_DIR}/osal_time.cpp
    ${PROJECT_SOURCE_DIR}/osal_timer.cpp
    ${PROJECT_SOURCE_DIR}/osal_math.cpp
    ${PROJECT_SOURCE_DIR}/osal_thread.cpp
    ${PROJECT_SOURCE_DIR}/osal_sleep.cpp
    ${PROJECT_SOURCE_DIR}/osal_rand.cpp
    ${PROJECT_SOURCE_DIR}/osal_socket.cpp
    ${PROJECT_SOURCE_DIR}/thread_manager.cpp
    ${PROJECT_SOURCE_DIR}/socket_info_manager.cpp
    ${PROJECT_SOURCE_DIR}/osal_exclusive_lock.cpp
    ${PROJECT_SOURCE_DIR}/osal_xml_parser.cpp
    ${PROJECT_SOURCE_DIR}/osal_xml_control.cpp
    ${PROJECT_SOURCE_DIR}/osal_winerror.cpp
    ${PROJECT_SOURCE_DIR}/osal_xml_creator.cpp
    ${PROJECT_SOURCE_DIR}/../common/osal_error.cpp
    )

set(PROJECT_NAME_FULL "SensCord OSAL")
set(PROJECT_DESCRIPTION "SensCord OSAL library")
configure_file(
    ${TEMPLATE_WINDOWS_DLL_VERSION}
    ${PROJECT_BINARY_DIR}/version.rc
    @ONLY)
list(APPEND LIB_OSAL_SOURCES
    ${PROJECT_BINARY_DIR}/version.rc)

# Add link libraries
set(LIB_OSAL_LINKS Ws2_32 xmllite Shlwapi Iphlpapi)

# Create osal.lib target.
add_library(${TARGET_NAME} SHARED ${LIB_OSAL_SOURCES})

set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "senscord_osal")

message(STATUS "osal link libraries -> ${LIB_OSAL_LINKS}")

# Link libraries.
target_link_libraries(${TARGET_NAME} PRIVATE ${LIB_OSAL_LINKS})

target_include_directories(${TARGET_NAME} PUBLIC
    $<BUILD_INTERFACE:${SSDK_BUILD_OUTPUT_INCLUDE_DIR}>
    $<BUILD_INTERFACE:${SSDK_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include>)
target_include_directories(${TARGET_NAME} PRIVATE ${OSAL_TOP_DIR})

# Create output destination directory
add_custom_command(TARGET  ${TARGET_NAME} PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E make_directory
                           ${SSDK_LIBRARY_OUT_DIR})

if(SENSCORD_TEST_OSAL)
  add_custom_command(TARGET  ${TARGET_NAME} PRE_BUILD
                     COMMAND ${CMAKE_COMMAND} -E make_directory
                             ${SSDK_BINARY_OUT_DIR})
endif()

# Install files.
install_osal(${TARGET_NAME})
