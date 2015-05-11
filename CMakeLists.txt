cmake_minimum_required(VERSION 2.8.12)
project(crossbow)

# For vim autocompletion
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# Declare build options
option(ENABLE_TESTS "Build and execute tests" ON)
option(ENABLE_STRING_TESTS "enable tests for crossbow string (currently only supported for clang on OS X)" OFF)
option(INFINIO_DEBUG "Build Infinio with debug logging enabled" OFF)

# Set default install paths
set(CMAKE_INSTALL_DIR cmake CACHE PATH "Installation directory for CMake files")
set(INCLUDE_INSTALL_DIR include CACHE PATH "Installation directory for header files")
set(LIB_INSTALL_DIR lib CACHE PATH "Installation directory for libraries")

# Set CMake modules path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Build with PIC by default
if(NOT CMAKE_POSITION_INDEPENDENT_CODE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# Set compile options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Werror")
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ferror-limit=5 -march=native")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    if (APPLE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmax-errors=5")
    else (APPLE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmax-errors=5 -march=native")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -ggdb")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb")
    endif(APPLE)
endif()

set(Crossbow_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR})

# Install Crossbow headers
install(DIRECTORY crossbow DESTINATION ${INCLUDE_INSTALL_DIR} FILES_MATCHING PATTERN "*.hpp")

# Build Crossbow libs
add_subdirectory(libs)

# Build Crossbow tests
if (${ENABLE_TESTS})
    enable_testing()
    add_subdirectory(test)
endif()

# Create cmake config file
configure_file(CrossbowConfig.cmake.in ${CMAKE_BINARY_DIR}/CrossbowConfig.cmake @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/CrossbowConfig.cmake DESTINATION ${CMAKE_INSTALL_DIR})
