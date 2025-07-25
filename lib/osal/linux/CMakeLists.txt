# SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
#
# SPDX-License-Identifier: Apache-2.0


################################################################################

################################################################################

# check libxml2
find_package(LibXml2 REQUIRED)
if (!LIBXML2_FOUND)
  message(FATAL_ERROR "Please install libxml2")
endif()

project(senscord_osal VERSION ${SensCord_VERSION})

# target name
set(TARGET_NAME ${PROJECT_NAME})

set_ssdk_library_output_dir(${SSDK_LIBRARY_OUT_DIR})

set(LIB_OSAL_SOURCES
    ${PROJECT_SOURCE_DIR}/osal_cond.cpp
    ${PROJECT_SOURCE_DIR}/osal_cond_monotonic.cpp
    ${PROJECT_SOURCE_DIR}/osal_file.cpp
    ${PROJECT_SOURCE_DIR}/osal_file_fopen.cpp
    ${PROJECT_SOURCE_DIR}/osal_directory.cpp
    ${PROJECT_SOURCE_DIR}/osal_directory_dylib.cpp
    ${PROJECT_SOURCE_DIR}/osal_math.cpp
    ${PROJECT_SOURCE_DIR}/osal_mem.cpp
    ${PROJECT_SOURCE_DIR}/osal_mutex.cpp
    ${PROJECT_SOURCE_DIR}/osal_mutex_timedlock.cpp
    ${PROJECT_SOURCE_DIR}/osal_rand.cpp
    ${PROJECT_SOURCE_DIR}/osal_sleep.cpp
    ${PROJECT_SOURCE_DIR}/osal_socket.cpp
    ${PROJECT_SOURCE_DIR}/osal_socket_buffersize.cpp
    ${PROJECT_SOURCE_DIR}/osal_stdio.cpp
    ${PROJECT_SOURCE_DIR}/osal_string.cpp
    ${PROJECT_SOURCE_DIR}/osal_string_strtoll.cpp
    ${PROJECT_SOURCE_DIR}/osal_thread.cpp
    ${PROJECT_SOURCE_DIR}/osal_thread_np.cpp
    ${PROJECT_SOURCE_DIR}/osal_dlloader.cpp
    ${PROJECT_SOURCE_DIR}/osal_time.cpp
    ${PROJECT_SOURCE_DIR}/osal_timer.cpp
    ${PROJECT_SOURCE_DIR}/osal_xml_parser.cpp
    ${PROJECT_SOURCE_DIR}/osal_xml_control.cpp
    ${PROJECT_SOURCE_DIR}/osal_exclusive_lock.cpp
    ${PROJECT_SOURCE_DIR}/osal_linuxerror.cpp
    ${PROJECT_SOURCE_DIR}/osal_xml_creator.cpp
    ${PROJECT_SOURCE_DIR}/socket_info_manager.cpp
    ${PROJECT_SOURCE_DIR}/../common/osal_error.cpp
    )

# Add link libraries
set(LIB_OSAL_LINKS pthread rt dl xml2)

# Create osal.lib target.
add_library(${TARGET_NAME} SHARED ${LIB_OSAL_SOURCES})

message(STATUS "osal link libraries -> ${LIB_OSAL_LINKS}")

# Link libraries.
target_link_libraries(${TARGET_NAME} PRIVATE ${LIB_OSAL_LINKS})

target_compile_options(${TARGET_NAME} PRIVATE ${SSDK_COMPILE_OPTIONS})

target_include_directories(${TARGET_NAME} PUBLIC
    $<BUILD_INTERFACE:${SSDK_BUILD_OUTPUT_INCLUDE_DIR}>
    $<BUILD_INTERFACE:${SSDK_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include>)
target_include_directories(${TARGET_NAME} PRIVATE ${OSAL_TOP_DIR})
target_include_directories(${TARGET_NAME} PRIVATE ${LIBXML2_INCLUDE_DIR})

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
