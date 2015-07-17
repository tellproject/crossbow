find_package(Ibverbs)
if(NOT ${Ibverbs_FOUND})
    message(STATUS "Did not find Ibverbs, InfinIO will not be built")
    return()
endif()

find_package(Rdmacm)
if(NOT ${Rdmacm_FOUND})
    message(STATUS "Did not find Rdma_cm, InfinIO will not be built")
    return()
endif()

find_package(Sparsehash)
if(NOT ${Sparsehash_FOUND})
    message(STATUS "Did not find Sparsehash, InfinIO will not be built")
    return()
endif()

find_package(TBB)
if(NOT ${TBB_FOUND})
    message(STATUS "Did not find TBB, InfinIO will not be built")
    return()
endif()

find_package(Boost COMPONENTS context REQUIRED)

find_package(Threads REQUIRED)

set(SRCS
    include/crossbow/infinio/BatchingMessageSocket.hpp
    include/crossbow/infinio/ByteBuffer.hpp
    include/crossbow/infinio/Endpoint.hpp
    include/crossbow/infinio/ErrorCode.hpp
    include/crossbow/infinio/EventProcessor.hpp
    include/crossbow/infinio/Fiber.hpp
    include/crossbow/infinio/InfinibandBuffer.hpp
    include/crossbow/infinio/InfinibandLimits.hpp
    include/crossbow/infinio/InfinibandService.hpp
    include/crossbow/infinio/InfinibandSocket.hpp
    include/crossbow/infinio/MessageId.hpp
    include/crossbow/infinio/RpcClient.hpp
    include/crossbow/infinio/RpcServer.hpp
    src/AddressHelper.cpp
    src/AddressHelper.hpp
    src/DeviceContext.hpp
    src/DeviceContext.cpp
    src/Endpoint.cpp
    src/EventProcessor.cpp
    src/Fiber.cpp
    src/InfinibandBuffer.cpp
    src/InfinibandService.cpp
    src/InfinibandSocket.cpp
    src/RpcClient.cpp
    src/WorkRequestId.hpp
)

# Add the InfinIO library
add_library(crossbow_infinio STATIC ${SRCS})
target_include_directories(crossbow_infinio PUBLIC
        $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# Link against Crossbow
target_include_directories(crossbow_infinio PRIVATE ${Crossbow_INCLUDE_DIRS})
target_link_libraries(crossbow_infinio PRIVATE crossbow_logger)

# Link against Threads
target_link_libraries(crossbow_infinio PUBLIC ${CMAKE_THREAD_LIBS_INIT})

# Link against Boost
target_include_directories(crossbow_infinio PUBLIC ${Boost_INCLUDE_DIRS})
target_link_libraries(crossbow_infinio PUBLIC ${Boost_LIBRARIES})

# Link against ibverbs
target_include_directories(crossbow_infinio PUBLIC ${Ibverbs_INCLUDE_DIRS})
target_link_libraries(crossbow_infinio PUBLIC ${Ibverbs_LIBRARIES})

# Link against rdmacm
target_include_directories(crossbow_infinio PUBLIC ${Rdmacm_INCLUDE_DIRS})
target_link_libraries(crossbow_infinio PUBLIC ${Rdmacm_LIBRARIES})

# Link against sparsehash
target_include_directories(crossbow_infinio PUBLIC ${Sparsehash_INCLUDE_DIRS})

# Link against TBB
target_include_directories(crossbow_infinio PUBLIC ${TBB_INCLUDE_DIRS})
target_link_libraries(crossbow_infinio PUBLIC ${TBB_LIBRARIES})

# Install the library
install(TARGETS crossbow_infinio
        EXPORT CrossbowInfinIOTargets
        ARCHIVE DESTINATION ${LIB_INSTALL_DIR})

# Build the echo server example application
add_executable(echoserver example/EchoServer.cpp)
target_include_directories(echoserver PRIVATE ${Crossbow_INCLUDE_DIRS})
target_link_libraries(echoserver PRIVATE crossbow_infinio)

# Build the ping client example application
add_executable(pingclient example/PingClient.cpp)
target_include_directories(pingclient PRIVATE ${Crossbow_INCLUDE_DIRS})
target_link_libraries(pingclient PRIVATE crossbow_infinio)

# Install Crossbow InfinIO headers
install(DIRECTORY include/crossbow DESTINATION ${INCLUDE_INSTALL_DIR} FILES_MATCHING PATTERN "*.hpp")

# Install the cmake target
install(EXPORT CrossbowInfinIOTargets DESTINATION ${CMAKE_INSTALL_DIR})
