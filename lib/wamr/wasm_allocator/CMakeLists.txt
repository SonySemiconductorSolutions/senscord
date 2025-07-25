# SPDX-FileCopyrightText: 2023-2024 Sony Semiconductor Solutions Corporation
#
# SPDX-License-Identifier: Apache-2.0


################################################################################

################################################################################

set(PROJECT_NAME wasm_allocator)
project(${PROJECT_NAME} VERSION ${SensCord_VERSION})

# target name
set(TARGET_NAME ${PROJECT_NAME})

set(TARGET_SOURCES
    src/wasm_memory_allocator.cpp
)

if(WIN32)
  set(PROJECT_NAME_FULL "WASM Memory Allocator")
  set(PROJECT_DESCRIPTION "WASM memory allocator library for SensCord")
  configure_file(
      ${TEMPLATE_WINDOWS_DLL_VERSION}
      ${PROJECT_BINARY_DIR}/version.rc
      @ONLY)
  list(APPEND TARGET_SOURCES
      ${PROJECT_BINARY_DIR}/version.rc)
endif()

add_library(${TARGET_NAME} SHARED ${TARGET_SOURCES})

if(WIN32)
  # Set target folder name
  set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "allocator")
endif()

target_link_libraries(${TARGET_NAME} senscord libiwasm)

target_compile_options(${TARGET_NAME} PRIVATE ${SSDK_COMPILE_OPTIONS})

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SSDK_ALLOCATOR_OUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${TARGET_NAME}> ${SSDK_ALLOCATOR_OUT_DIR}
)
install_allocator(${TARGET_NAME})
