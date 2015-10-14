cmake_minimum_required(VERSION 2.8.12)
project(crossbow)

# Declare build options
option(ENABLE_TESTS "Build and execute tests" ON)
option(ENABLE_STRING_TESTS "enable tests for crossbow string (currently only supported for clang on OS X)" OFF)

# Set default install paths
set(CMAKE_INSTALL_DIR cmake CACHE PATH "Installation directory for CMake files")
set(INCLUDE_INSTALL_DIR include CACHE PATH "Installation directory for header files")
set(LIB_INSTALL_DIR lib CACHE PATH "Installation directory for libraries")

# Set CMake modules path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Set compile options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
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

set(Crossbow_DIR ${CMAKE_CURRENT_BINARY_DIR} CACHE PATH "Path to the crossbow binaries and configuration")
set(Crossbow_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} CACHE PATH "Path to the crossbow include headers")

# Install Crossbow headers
install(DIRECTORY crossbow DESTINATION ${INCLUDE_INSTALL_DIR} FILES_MATCHING PATTERN "*.hpp")

set(CROSSBOW_PUBLIC_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/crossbow")
# Build Crossbow libs
add_subdirectory(libs)

# Build Crossbow tests
if (${ENABLE_TESTS})
    enable_testing()
    add_subdirectory(test)
endif()

# Create cmake config file
configure_file(CrossbowConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/CrossbowConfig.cmake @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/CrossbowConfig.cmake DESTINATION ${CMAKE_INSTALL_DIR})

# Documentation
if(BUILD_GLOBAL_DOCS)
    set(TELL_PUBLIC_HDRS "${TELL_PUBLIC_HDRS} ${CROSSBOW_PUBLIC_HDRS}" PARENT_SCOPE)
else()
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
        add_custom_target(doc
            ${DOXYGEN_EXECUTABLE}
            ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation")
    endif()
endif()
