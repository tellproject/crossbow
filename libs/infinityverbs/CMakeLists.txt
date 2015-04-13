find_package(Ibverbs)
if (NOT ${Ibverbs_FOUND})
    message(STATUS "Did not find Ibverbs, Infinityverbs will not be built")
    return()
endif()

set(SRCS
    src/core/Configuration.hpp
    src/core/Context.cpp
    src/core/Context.hpp
    src/core/DirectMemoryAccessStore.cpp
    src/core/DirectMemoryAccessStore.hpp
    src/core/MemoryTokenStore.cpp
    src/core/MemoryTokenStore.hpp
    src/core/SerializedMemoryToken.hpp
    src/InfinityVerbs.hpp
    src/memory/AtomicValue.cpp
    src/memory/AtomicValue.hpp
    src/memory/MemoryRegion.hpp
    src/memory/MemoryRegionType.hpp
    src/memory/MemoryToken.cpp
    src/memory/MemoryToken.hpp
    src/memory/RdmaBuffer.cpp
    src/memory/RdmaBuffer.hpp
    src/queues/QueuePair.cpp
    src/queues/QueuePair.hpp
    src/queues/QueuePairFactory.cpp
    src/queues/QueuePairFactory.hpp
    src/tools/AddressResolver.cpp
    src/tools/AddressResolver.hpp
    src/tools/Lock.cpp
    src/tools/Lock.hpp
    src/tools/ThreadControl.cpp
    src/tools/ThreadControl.hpp)

# Add InfinityVerbs library
add_library(crossbow_infinityverbs STATIC ${SRCS})
target_include_directories(crossbow_infinityverbs PUBLIC
        $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)

# Link against ibverbs
target_include_directories(crossbow_infinityverbs PUBLIC ${Ibverbs_INCLUDE_DIRS})
target_link_libraries(crossbow_infinityverbs PUBLIC ${Ibverbs_LIBRARIES})

# Add InfinityVerbs examples
add_executable(infinityverbs_sample examples/src/main.cpp)
target_link_libraries(infinityverbs_sample crossbow_infinityverbs)

# Install the library
install(TARGETS crossbow_infinityverbs
        EXPORT CrossbowInfinityVerbsTargets
        ARCHIVE DESTINATION ${LIB_INSTALL_DIR})

# Install the header files
foreach(source ${SRCS})
    if (${source} MATCHES ".*\\.hpp$")
        install(FILES ${source} DESTINATION ${INCLUDE_INSTALL_DIR}/crossbow/infinityverbs)
    endif()
endforeach()

# Install the cmake target
install(EXPORT CrossbowInfinityVerbsTargets DESTINATION ${CMAKE_INSTALL_DIR})
