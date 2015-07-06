find_package(Boost REQUIRED)

set(SRCS
    include/crossbow/logger.hpp
    src/logger.cpp
)

# Add the Logger library
add_library(crossbow_logger STATIC ${SRCS})
target_include_directories(crossbow_logger PUBLIC
        $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# Link against Crossbow
target_include_directories(crossbow_logger PRIVATE ${Crossbow_INCLUDE_DIRS})

# Link against Boost
target_include_directories(crossbow_logger PUBLIC ${Boost_INCLUDE_DIRS})

# Install the library
install(TARGETS crossbow_logger
        EXPORT CrossbowLoggerTargets
        ARCHIVE DESTINATION ${LIB_INSTALL_DIR})

# Install Crossbow Logger headers
install(DIRECTORY include/crossbow DESTINATION ${INCLUDE_INSTALL_DIR} FILES_MATCHING PATTERN "*.hpp")

# Install the cmake target
install(EXPORT CrossbowLoggerTargets DESTINATION ${CMAKE_INSTALL_DIR})
