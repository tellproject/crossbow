cmake_minimum_required(VERSION 2.8.12)
project(crossbow)

# Declare build options
option(ENABLE_TESTS "Build and execute tests (currently only supported on Clang/Mac OS X)" OFF)

# Set default install paths
set(CMAKE_INSTALL_DIR cmake CACHE PATH "Installation directory for CMake files")
set(INCLUDE_INSTALL_DIR include CACHE PATH "Installation directory for header files")
set(LIB_INSTALL_DIR lib CACHE PATH "Installation directory for libraries")

# Set CMake modules path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Set compile options
if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -stdlib=libc++ -fPIC -Werror -march=native -ferror-limit=5")
else(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -fPIC -Werror -fmax-errors=5 -march=native")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -ggdb")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb")
endif(APPLE)

# Install Crossbow headers
install(DIRECTORY crossbow DESTINATION ${INCLUDE_INSTALL_DIR} FILES_MATCHING PATTERN "*.hpp")

# Build Crossbow libs
add_subdirectory(libs)

# Build Crossbow tests
if(${ENABLE_TESTS})
    enable_testing()
    add_subdirectory(test)
endif()

# Create cmake config file
configure_file(CrossbowConfig.cmake.in ${CMAKE_BINARY_DIR}/CrossbowConfig.cmake @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/CrossbowConfig.cmake DESTINATION ${CMAKE_INSTALL_DIR})
