find_package(Ibverbs)
if (${IBVERBS_FOUND})
    include_directories(${Ibverbs_INCLUDE_DIR})
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
    add_library(crossbow_infinityverbs ${SRCS})
    target_link_libraries(crossbow_infinityverbs ${Ibverbs_LIBRARY})

    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_executable(infinityverbs_sample examples/src/main.cpp)
    target_link_libraries(infinityverbs_sample crossbow_infinityverbs ${Ibverbs_LIBRARY})

    # for installing
    foreach(source ${SRCS})
        if (${source} MATCHES ".*\\.hpp$")
            install(FILES ${source} DESTINATION include/crossbow/infinityverbs)
        endif()
    endforeach()
else()
    message(STATUS "Did not find Ibverns, Infinityverbs will not be built")
endif()

