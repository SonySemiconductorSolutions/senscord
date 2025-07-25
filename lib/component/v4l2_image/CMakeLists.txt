# SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
#
# SPDX-License-Identifier: Apache-2.0


################################################################################

################################################################################

set(PROJECT_NAME component_v4l2_image)
project(${PROJECT_NAME} VERSION ${SensCord_VERSION})

# target name
set(TARGET_NAME ${PROJECT_NAME})

# output directory
set(OUTPUT_INCLUDE_DIR v4l2_image)

### Sources
# Public header directory
set(PUBLIC_HEADER_DIR ${PROJECT_SOURCE_DIR}/include/senscord/${OUTPUT_INCLUDE_DIR})

# Public header files.
file(GLOB PUBLIC_HEADER_FILES ${PUBLIC_HEADER_DIR}/*.h)

# Add source codes.
set(PROJECT_SOURCE
    ${PUBLIC_HEADER_FILES}
    src/v4l2_accessor.cpp
    src/v4l2_accessor.h
    src/v4l2_image_stream_source.cpp
    src/v4l2_image_stream_source.h
    src/v4l2_image_stream_source_factory.cpp)

# Create library target.
add_library(${TARGET_NAME} SHARED ${PROJECT_SOURCE})

# Include directory.
target_include_directories(${TARGET_NAME} PRIVATE
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_SOURCE_DIR}/include)

# link libraries.
target_link_libraries(${TARGET_NAME} senscord)

# Create output destination directory
add_custom_command(TARGET  ${TARGET_NAME} PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E make_directory
                           ${SSDK_COMPONENT_OUT_DIR})

# Install files.
install_component(${TARGET_NAME}
                  ${PROJECT_SOURCE_DIR}/config/component_v4l2_image.xml)

### Copy Config file.
add_configure_file(${TARGET_NAME}
                   ${PROJECT_SOURCE_DIR}/config/component_v4l2_image.xml
                   ${SSDK_COMPONENT_OUT_DIR})

### Public header function.
add_public_header(${TARGET_NAME}
                  ${PROJECT_SOURCE_DIR}/include
                  ${OUTPUT_INCLUDE_DIR})
