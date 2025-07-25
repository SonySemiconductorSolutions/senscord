# SPDX-FileCopyrightText: 2023-2024 Sony Semiconductor Solutions Corporation
#
# SPDX-License-Identifier: Apache-2.0


################################################################################

################################################################################

project(senscord_wamr VERSION ${SensCord_VERSION})

# WAMR_ROOT_DIR
set(WAMR_ROOT_DIR "${CMAKE_BINARY_DIR}/wasm-micro-runtime" CACHE STRING "")
if(WAMR_ROOT_DIR)
  get_filename_component(WAMR_ROOT_DIR ${WAMR_ROOT_DIR} ABSOLUTE)
endif()
message(STATUS "WAMR_ROOT_DIR=${WAMR_ROOT_DIR}")
if(NOT EXISTS ${WAMR_ROOT_DIR})
  message(FATAL_ERROR "WAMR_ROOT_DIR=\"${WAMR_ROOT_DIR}\" does not exist")
endif()

# WAMR_IWASM_LIBRARY
find_library(
    WAMR_IWASM_LIBRARY NAMES libiwasm.so
    HINTS ${CMAKE_INSTALL_PREFIX}/lib)
message(STATUS "WAMR_IWASM_LIBRARY=${WAMR_IWASM_LIBRARY}")

# WASI_SDK_DIR
if(SENSCORD_TEST_CORE OR SENSCORD_SAMPLE)
  set(WASI_SDK_DIR "/opt/wasi-sdk" CACHE STRING "")
  if(WASI_SDK_DIR)
    get_filename_component(WASI_SDK_DIR ${WASI_SDK_DIR} ABSOLUTE)
  endif()
  message(STATUS "WASI_SDK_DIR=${WASI_SDK_DIR}")
  if(NOT EXISTS ${WASI_SDK_DIR})
    message(FATAL_ERROR "WASI_SDK_DIR=\"${WASI_SDK_DIR}\" does not exist")
  endif()
endif()

#################################################
# runtime settings

string(TOLOWER ${CMAKE_HOST_SYSTEM_NAME} WAMR_BUILD_PLATFORM)
if(APPLE)
  add_definitions(-DBH_PLATFORM_DARWIN)
endif()

# Reset default linker flags
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

# Set WAMR_BUILD_TARGET, currently values supported are:
# "X86_64", "AMD_64", "X86_32", "AARCH64[sub]", "ARM[sub]", "THUMB[sub]",
# "MIPS", "XTENSA", "RISCV64[sub]", "RISCV32[sub]"
if(NOT DEFINED WAMR_BUILD_TARGET)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)")
    set(WAMR_BUILD_TARGET "AARCH64")
  elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "riscv64")
    set(WAMR_BUILD_TARGET "RISCV64")
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    # Build as X86_64 by default in 64-bit platform
    set(WAMR_BUILD_TARGET "X86_64")
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    # Build as X86_32 by default in 32-bit platform
    set(WAMR_BUILD_TARGET "X86_32")
  else()
    message(SEND_ERROR "Unsupported build target platform!")
  endif()
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(NOT DEFINED WAMR_BUILD_INTERP)
  set(WAMR_BUILD_INTERP 1)
endif()
if(NOT DEFINED WAMR_BUILD_FAST_INTERP)
  set(WAMR_BUILD_FAST_INTERP 1)
endif()
if(NOT DEFINED WAMR_BUILD_AOT)
  set(WAMR_BUILD_AOT 1)
endif()
if(NOT DEFINED WAMR_BUILD_LIBC_BUILTIN)
  set(WAMR_BUILD_LIBC_BUILTIN 1)
endif()
if(NOT DEFINED WAMR_BUILD_LIBC_WASI)
  set(WAMR_BUILD_LIBC_WASI 1)
endif()

if(NOT DEFINED WAMR_BUILD_LIB_PTHREAD AND
    NOT DEFINED WAMR_BUILD_LIB_WASI_THREADS)
  set(WAMR_BUILD_LIB_PTHREAD 1)
endif()

set(WAMR_BUILD_THREAD_MGR 1)
set(WAMR_BUILD_BULK_MEMORY 1)
set(WAMR_BUILD_SHARED_MEMORY 1)
set(WAMR_BUILD_MODULE_INST_CONTEXT 1)

# compiling and linking flags
if(NOT (CMAKE_C_COMPILER MATCHES ".*clang.*" OR CMAKE_C_COMPILER_ID MATCHES ".*Clang"))
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wformat -Wformat-security -Wshadow")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wconversion -Wsign-conversion")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter")

#################################################
# libiwasm.so

include(${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)

if(NOT EXISTS ${WAMR_IWASM_LIBRARY})
  add_library(libiwasm SHARED ${WAMR_RUNTIME_LIB_SOURCE})
  set_target_properties(libiwasm PROPERTIES OUTPUT_NAME iwasm)
  target_include_directories(libiwasm PUBLIC ${IWASM_DIR}/include)

  add_custom_command(TARGET libiwasm POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory ${SSDK_LIBRARY_OUT_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libiwasm> ${SSDK_LIBRARY_OUT_DIR})
  install(TARGETS libiwasm
      EXPORT ${SSDK_EXPORT_NAME}
      RUNTIME DESTINATION ${SSDK_BIN_INSTALL_DIR}
      LIBRARY DESTINATION ${SSDK_LIBRARY_INSTALL_DIR}
      ARCHIVE DESTINATION ${SSDK_LIBRARY_INSTALL_DIR})
else()
  add_library(libiwasm SHARED IMPORTED)
  set_target_properties(libiwasm PROPERTIES IMPORTED_LOCATION ${WAMR_IWASM_LIBRARY})
  target_include_directories(libiwasm INTERFACE ${IWASM_DIR}/include)
endif()

#################################################
# iwasm

include(${SHARED_DIR}/utils/uncommon/shared_uncommon.cmake)

set(RUNTIME_SOURCE_ALL
    ${WAMR_ROOT_DIR}/product-mini/platforms/posix/main.c
    ${UNCOMMON_SHARED_SOURCE}
)

add_executable(senscord_iwasm ${RUNTIME_SOURCE_ALL})
set_target_properties(senscord_iwasm PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(senscord_iwasm libiwasm -lpthread -lm -ldl)

add_custom_command(TARGET senscord_iwasm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SSDK_BINARY_OUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:senscord_iwasm> ${SSDK_BINARY_OUT_DIR})
install(TARGETS senscord_iwasm
    EXPORT ${SSDK_EXPORT_NAME}
    RUNTIME DESTINATION ${SSDK_BIN_INSTALL_DIR})

#################################################
# native libraries

set(PROJECT_SOURCES
    src/senscord_wamr_api.cpp
    src/senscord_wamr_context.cpp
)

if(WIN32)
  set(PROJECT_NAME_FULL "SensCord WAMR")
  set(PROJECT_DESCRIPTION "SensCord WAMR native library")
  configure_file(
      ${TEMPLATE_WINDOWS_DLL_VERSION}
      ${PROJECT_BINARY_DIR}/version.rc
      @ONLY)
  list(APPEND PROJECT_SOURCES
      ${PROJECT_BINARY_DIR}/version.rc)
endif()

add_library(${PROJECT_NAME} SHARED ${PROJECT_SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE libiwasm senscord)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/../core)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SSDK_LIBRARY_OUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${SSDK_LIBRARY_OUT_DIR})
install(TARGETS ${PROJECT_NAME}
    EXPORT ${SSDK_EXPORT_NAME}
    RUNTIME DESTINATION ${SSDK_BIN_INSTALL_DIR}
    LIBRARY DESTINATION ${SSDK_LIBRARY_INSTALL_DIR}
    ARCHIVE DESTINATION ${SSDK_LIBRARY_INSTALL_DIR})

#################################################

add_subdirectory(wasm_allocator)

if(SENSCORD_SAMPLE)
  add_subdirectory(example)
endif()
