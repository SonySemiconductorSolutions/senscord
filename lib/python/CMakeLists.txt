# SPDX-FileCopyrightText: 2021-2023 Sony Semiconductor Solutions Corporation
#
# SPDX-License-Identifier: Apache-2.0


################################################################################

################################################################################

project(senscord_python VERSION ${SensCord_VERSION})

# Setting Build Options.
## add -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Change the output destination directory of the library.
set_ssdk_library_output_dir(${SSDK_LIBRARY_OUT_DIR})

# Sources.
set(PROJECT_SOURCES
    ${PROJECT_SOURCE_DIR}/src/serialize.cpp
)

if(WIN32)
  set(PROJECT_NAME_FULL "SensCord Python")
  set(PROJECT_DESCRIPTION "SensCord Python library")
  configure_file(
      ${TEMPLATE_WINDOWS_DLL_VERSION}
      ${PROJECT_BINARY_DIR}/version.rc
      @ONLY)
  list(APPEND PROJECT_SOURCES
      ${PROJECT_BINARY_DIR}/version.rc)
endif()

# Create target.
add_library(${PROJECT_NAME} SHARED ${PROJECT_SOURCES})

# Include directories.
target_include_directories(
    ${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/../core
)

# Link libraries.
set(LINK_LIBS)
## Add PythonLib
find_package(Python COMPONENTS Interpreter Development REQUIRED)
message(STATUS "Python version: ${Python_VERSION}")
message(STATUS "Python include directory: ${Python_INCLUDE_DIRS}")
if(Python_Development_FOUND)
  target_include_directories(${PROJECT_NAME} PRIVATE ${Python_INCLUDE_DIRS})
  if(WIN32)
    if(${Python_VERSION_MAJOR} EQUAL 3)
      string(REGEX REPLACE
        [[python3[0-9]*(_d)?.lib]]
        [[python3.lib]]
        PYTHON_LIBS "${Python_LIBRARIES}")
      list(APPEND LINK_LIBS "${PYTHON_LIBS}")
    else()
      list(APPEND LINK_LIBS "${Python_LIBRARIES}")
    endif()
  else()
    list(APPEND LINK_LIBS ${Python_LIBRARIES})
    target_compile_definitions(
        ${PROJECT_NAME} PRIVATE
        PYTHON_LIBRARIES=${Python_LIBRARIES})
  endif()
else()
  # Python library not found.
  if(UNIX)
    if(${Python_VERSION_MAJOR} EQUAL 3)
      message(FATAL_ERROR "Please install python3-dev")
    else()
      message(FATAL_ERROR "Please install python-dev")
    endif()
  else()
    message(FATAL_ERROR "Please install Python")
  endif()
endif()

list(APPEND LINK_LIBS senscord)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LINK_LIBS})

message(STATUS "${PROJECT_NAME} link libraries -> ${LINK_LIBS}")

# Set target folder name.
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ${PROJECT_NAME})

# Create output destination directory.
add_custom_command(TARGET  ${PROJECT_NAME} PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E make_directory
                   ${SSDK_LIBRARY_OUT_DIR})

# Install.
if (WIN32)
  install(TARGETS             ${PROJECT_NAME}
          RUNTIME DESTINATION ${SSDK_BIN_INSTALL_DIR})
else()
  install(TARGETS             ${PROJECT_NAME}
          LIBRARY DESTINATION ${SSDK_LIBRARY_INSTALL_DIR})
endif()
