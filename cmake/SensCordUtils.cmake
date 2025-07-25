# SPDX-FileCopyrightText: 2017-2023 Sony Semiconductor Solutions Corporation
#
# SPDX-License-Identifier: Apache-2.0

################################################################################

# Include modules for parameter analysis
include(CMakeParseArguments)

### Macro

# Macro to set the output destination of the result of
# building the executable file.
macro(set_ssdk_runtime_output_dir value)
  # Destination setting for visual studio and xcode.
  if(WIN32)
    set(BUILD_TYPES DEBUG RELEASE MINSIZEREL RELWITHDEBINFO)
    foreach(build_type ${BUILD_TYPES})
      set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${build_type} ${value})
      set(CMAKE_PDB_OUTPUT_DIRECTORY_${build_type}     ${value})
      set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${build_type} ${value})
    endforeach()
  elseif(APPLE)
    set(BUILD_TYPES DEBUG RELEASE)
    foreach(build_type ${BUILD_TYPES})
      set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${build_type} ${value})
    endforeach()
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${value})
  else()
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${value})
  endif()
endmacro()

# Macro to set the output destination of the result of
# building the archive file.
macro(set_ssdk_archive_output_dir value)
  # Destination setting for visual studio and xcode.
  if(WIN32)
    set(BUILD_TYPES DEBUG RELEASE MINSIZEREL RELWITHDEBINFO)
    foreach(build_type ${BUILD_TYPES})
      set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${build_type} ${value})
      set(CMAKE_PDB_OUTPUT_DIRECTORY_${build_type}     ${value})
    endforeach()
  elseif(APPLE)
    set(BUILD_TYPES DEBUG RELEASE)
    foreach(build_type ${BUILD_TYPES})
      set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${build_type} ${value})
    endforeach()
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${value})
  else()
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${value})
  endif()
endmacro()

# Macro to set the output destination of the result of
# building the shared library file.
macro(set_ssdk_shared_library_output_dir value)
  # Destination setting for xcode.
  if(APPLE)
    set(BUILD_TYPES DEBUG RELEASE)
    foreach(build_type ${BUILD_TYPES})
      set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${build_type} ${value})
    endforeach()
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${value})
  elseif(NOT WIN32)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${value})
  endif()
endmacro()

# Macro to set the output destination of the result of
# building the library file.
macro(set_ssdk_library_output_dir value)
  # Destination setting for visual studio.
  if(WIN32)
    set_ssdk_runtime_output_dir(${value})
  else()
    set_ssdk_shared_library_output_dir(${value})
  endif()
endmacro()

macro(set_target_folder target_name folder_name)
  if (WIN32)
    # Set target folder name
    set_target_properties("${target_name}" PROPERTIES FOLDER "${folder_name}")
  endif()
endmacro()

### Function

# Function to register osal's test.
function(add_osal_test target_name test_command test_label)
  add_test(NAME ${target_name} COMMAND ${test_command})
  if(WIN32)
    set(tmp_path "Path=$ENV{Path};${SSDK_BUILD_OUTPUT_DIR}/lib")
    string(REPLACE ";" "\\;" env_path "${tmp_path}")
    set_tests_properties(${target_name} PROPERTIES
                                        LABELS ${test_label}
                                        ENVIRONMENT "Path=${env_path}")
  else()
    set_tests_properties(${target_name} PROPERTIES LABELS ${test_label})
  endif()
endfunction()

# Designation of installation of senscord library
function(install_senscord target_name)

  if (WIN32)
    # Install files.
    install(TARGETS ${target_name}
            EXPORT ${SSDK_EXPORT_NAME}
            ARCHIVE       DESTINATION ${SSDK_LIBRARY_INSTALL_DIR}
            RUNTIME       DESTINATION ${SSDK_BIN_INSTALL_DIR}
            INCLUDES      DESTINATION ${SSDK_INCLUDE_INSTALL_DIR}
            PUBLIC_HEADER DESTINATION ${SSDK_INCLUDE_SENSCORD_INSTALL_DIR})
  else()
    # Install files.
    install(TARGETS ${target_name}
            EXPORT ${SSDK_EXPORT_NAME}
            LIBRARY       DESTINATION ${SSDK_LIBRARY_INSTALL_DIR}
            INCLUDES      DESTINATION ${SSDK_INCLUDE_INSTALL_DIR}
            PUBLIC_HEADER DESTINATION ${SSDK_INCLUDE_SENSCORD_INSTALL_DIR})
  endif()
endfunction()

# Designation of installation of osal library
function(install_osal target_name)
  if (WIN32)
    # Install files.
    install(TARGETS ${target_name}
        EXPORT ${SSDK_EXPORT_NAME}
        RUNTIME DESTINATION ${SSDK_OSAL_INSTALL_DIR}
        ARCHIVE DESTINATION ${SSDK_LIBRARY_INSTALL_DIR})
  else()
    # Install files.
    install(TARGETS ${target_name}
        EXPORT ${SSDK_EXPORT_NAME}
        LIBRARY DESTINATION ${SSDK_OSAL_INSTALL_DIR})
  endif()
endfunction()

# Designation of installation of static library
function(install_static_libs target_name)
  install(TARGETS ${target_name}
      EXPORT ${SSDK_EXPORT_NAME}
      LIBRARY DESTINATION ${SSDK_STATIC_LIBRARY_INSTALL_DIR}
      ARCHIVE DESTINATION ${SSDK_STATIC_LIBRARY_INSTALL_DIR})
endfunction()

# Designation of installation of component
function(install_component target_name config_file)
  if (WIN32)
    # Install files.
    install(TARGETS ${target_name}
            RUNTIME DESTINATION ${SSDK_COMPONENT_INSTALL_DIR}
            ARCHIVE DESTINATION ${SSDK_COMPONENT_INSTALL_DIR})
  else()
    # Install files.
    install(TARGETS ${target_name}
            LIBRARY DESTINATION ${SSDK_COMPONENT_INSTALL_DIR})
  endif()

  install(FILES       ${config_file}
          DESTINATION ${SSDK_COMPONENT_INSTALL_DIR})
endfunction()

# Designation of installation of allocator
function(install_allocator target_name)
  if (WIN32)
    # Install files.
    install(TARGETS ${target_name}
            RUNTIME DESTINATION ${SSDK_ALLOCATOR_INSTALL_DIR}
            ARCHIVE DESTINATION ${SSDK_ALLOCATOR_INSTALL_DIR})
  else()
    # Install files.
    install(TARGETS ${target_name}
            LIBRARY DESTINATION ${SSDK_ALLOCATOR_INSTALL_DIR})
  endif()
endfunction()

# Designation of installation of recorder
function(install_recorder target_name)
  if (WIN32)
    # Install files.
    install(TARGETS ${target_name}
            RUNTIME DESTINATION ${SSDK_RECORDER_INSTALL_DIR}
            ARCHIVE DESTINATION ${SSDK_RECORDER_INSTALL_DIR})
  else()
    # Install files.
    install(TARGETS ${target_name}
            LIBRARY DESTINATION ${SSDK_RECORDER_INSTALL_DIR})
  endif()
endfunction()

# Designation of installation of connection
function(install_connection target_name)
  if (WIN32)
    # Install files.
    install(TARGETS ${target_name}
            RUNTIME DESTINATION ${SSDK_CONNECTION_INSTALL_DIR}
            ARCHIVE DESTINATION ${SSDK_CONNECTION_INSTALL_DIR})
  else()
    # Install files.
    install(TARGETS ${target_name}
            LIBRARY DESTINATION ${SSDK_CONNECTION_INSTALL_DIR})
  endif()
endfunction()

# Create directory pre Build.
function(make_output_dir_prebuild target_name output_dir)
  # Create output destination directory
  add_custom_command(TARGET  ${target_name} PRE_BUILD
                     COMMAND ${CMAKE_COMMAND} -E make_directory
                     ${output_dir})
endfunction()

# Add build targets to copy files
function(add_configure_file target_name source_file dest_dir)
  # Create output destination directory
  make_output_dir_prebuild(${target_name} ${dest_dir})

  # Create custom target
  add_custom_command(TARGET  ${target_name} POST_BUILD
                     COMMAND ${CMAKE_COMMAND}
                             -DSOURCE_FILE=${source_file}
                             -DDEST_DIR=${dest_dir}
                             -P ${SSDK_MODULE_DIR}/CopyFileIfNoExist.cmake)
endfunction()

# Generate senscord.xml to build targets
function(generate_senscord_xml target_name source_file dest_dir)
  # Create output destination directory
  make_output_dir_prebuild(${target_name} ${dest_dir})

  # Create custom target
  add_custom_command(TARGET  ${target_name} POST_BUILD
                     COMMAND ${CMAKE_COMMAND}
                             -DSOURCE_FILE=${source_file}
                             -DDEST_DIR=${dest_dir}
                             -DSENSCORD_COMPONENT_PSEUDO=${SENSCORD_COMPONENT_PSEUDO}
                             -DSENSCORD_COMPONENT_V4L2=${SENSCORD_COMPONENT_V4L2}
                             -DSENSCORD_PLAYER_SKV=${SENSCORD_PLAYER_SKV}
                             -P ${SSDK_MODULE_DIR}/GenerateSensCordXml.cmake)
endfunction()

# Register the publication header of the component
function(add_public_header target_name include_dir header_dir)
  set(componet_header_dir ${include_dir}/senscord/${header_dir})

  # Public header files.
  file(GLOB public_header_files ${componet_header_dir}/*.h)

  # Create output destination directory
  make_output_dir_prebuild(${target_name} ${SSDK_HEADER_OUT_DIR})

  # Add target to copy header
  add_custom_command(TARGET  ${target_name} POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E copy_directory
                             ${componet_header_dir}
                             ${SSDK_HEADER_OUT_DIR}/${header_dir})

  # install header files
  install(FILES       ${public_header_files}
          DESTINATION ${SSDK_INCLUDE_SENSCORD_INSTALL_DIR}/${header_dir})

endfunction()

# Create a custom target to run cpplint
# - FILES     # It is possible to specify multiple file paths to be analyzed,
#             # use of wild cards (must)
# - ROOT_PATH # Path specified for the --root option (optional)
# - DEPENDS   # Custom target name of dependent cpplint (optional)
function(add_cpplint_target target_name)
  cmake_parse_arguments(cpplint "" "ROOT_PATH" "DEPENDS;FILES" ${ARGN})

  if (NOT cpplint_FILES)
    message(FATAL_ERROR "Always be sure to define the FILES")
  endif()

  # Setting the root path option of the header
  if (cpplint_ROOT_PATH)
    set(rootpath_option "--root=${cpplint_ROOT_PATH}")
  endif()

  # Register target of cpplint
  add_custom_target(cpplint-${target_name}
      COMMAND python ${SSDK_TOP_DIR}/tools/cpplint/cpplint.py
      --extensions=h,cpp ${rootpath_option} ${cpplint_FILES})

  # Dependency target setting
  if (cpplint_DEPENDS)
    add_dependencies(cpplint-${target_name} ${cpplint_DEPENDS})
  endif()

  add_dependencies(cpplint-all cpplint-${target_name})
endfunction()

# Setting build warning level
function(set_build_warning_level level)
  set(warning_level "")
  set(CMAKE_CXX_FLAGS_WORK "${CMAKE_CXX_FLAGS}")

  if (level STREQUAL ${WARNING_LEVEL_CORE})
    if (WIN32)
      set(warning_level "/W4")
    else()
      set(warning_level "-Wall")
    endif()
  else()
    if (WIN32)
      set(warning_level "/W3")
    else()
      set(warning_level "")
      string(REGEX REPLACE "-Wall" "" CMAKE_CXX_FLAGS_WORK "${CMAKE_CXX_FLAGS_WORK}")
    endif()
  endif()

  if (WIN32)
    if (CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
      string(REGEX REPLACE "/W[0-4]" "${warning_level}" CMAKE_CXX_FLAGS_WORK "${CMAKE_CXX_FLAGS_WORK}")
    endif()
  endif()

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_WORK} ${warning_level}" PARENT_SCOPE)
endfunction()
