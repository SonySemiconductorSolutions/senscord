# SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
#
# SPDX-License-Identifier: Apache-2.0

################################################################################

set(OSAL_TOP_DIR ${CMAKE_CURRENT_SOURCE_DIR})

### Add build target
if(UNIX)
  if(ANDROID)
    set(TARGET_ANDROID ON)
    add_definitions(-DTARGET_ANDROID)
    add_subdirectory(android)
  elseif(APPLE)
    set(TARGET_APPLE ON)
    add_definitions(-DTARGET_APPLE)
    add_definitions(-D_FILE_OFFSET_BITS=64)
    add_subdirectory(darwin)
  else()
    set(TARGET_LINUX ON)
    add_definitions(-DTARGET_LINUX)
    add_definitions(-D_FILE_OFFSET_BITS=64)
    add_subdirectory(linux)
  endif()
endif()

if(WIN32)
  set(TARGET_WINDOWS ON)
  add_definitions(-DTARGET_WINDOWS)
  add_subdirectory(windows)
endif()
