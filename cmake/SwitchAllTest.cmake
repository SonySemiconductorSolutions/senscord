# SPDX-FileCopyrightText: 2017-2023 Sony Semiconductor Solutions Corporation
#
# SPDX-License-Identifier: Apache-2.0

################################################################################

# Script to register new test.
#  - OPTION_SWITCH  : Specify switching of options with ON / OFF.
#  - CMAKELISTS_DIR : Where CMakeLists.txt is stored.

if (NOT "${OPTION_SWITCH}" STREQUAL "ON" AND
    NOT "${OPTION_SWITCH}" STREQUAL "OFF")
  message(FATAL_ERROR "Invalid option was used.")
endif()

# Setting options to be enabled.
set(CMAKE_ARGS
    # Option setting for test code.
    "-DSENSCORD_TEST_CORE=${OPTION_SWITCH}"
    "-DSENSCORD_TEST_OSAL=${OPTION_SWITCH}"
    )

# Specifying the build directory.
if (NOT DEFINED CMAKELISTS_DIR)
  set(CMAKELISTS_DIR ".")
endif()

execute_process(COMMAND ${CMAKE_COMMAND} ${CMAKE_ARGS} ${CMAKELISTS_DIR})
