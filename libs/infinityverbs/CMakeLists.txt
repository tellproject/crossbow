find_package(Ibverbs)
if (${Ibverbs_FOUND})
    include_directories(${Ibverbs_INCLUDE_DIRS})
    set(SRCS
        core/Configuration.hpp
        core/Context.cpp
        core/Context.hpp
        core/DirectMemoryAccessStore.cpp
        core/DirectMemoryAccessStore.hpp
        core/MemoryTokenStore.cpp
        core/MemoryTokenStore.hpp
        core/SerializedMemoryToken.hpp
        InfinityVerbs.hpp
        memory/AtomicValue.cpp
        memory/AtomicValue.hpp
        memory/MemoryRegion.hpp
        memory/MemoryRegionType.hpp
        memory/MemoryToken.cpp
        memory/MemoryToken.hpp
        memory/RdmaBuffer.cpp
        memory/RdmaBuffer.hpp
        queues/QueuePair.cpp
        queues/QueuePair.hpp
        queues/QueuePairFactory.cpp
        queues/QueuePairFactory.hpp
        tools/AddressResolver.cpp
        tools/AddressResolver.hpp
        tools/Lock.cpp
        tools/Lock.hpp
        tools/ThreadControl.cpp
        tools/ThreadControl.hpp)
    add_library(crossbow_infinityverbs ${SRCS})
    target_link_libraries(crossbow_infinityverbs ${Ibverbs_LIBRARIES})

    # for installing
    foreach(source ${SRCS})
        if (${source} MATCHES ".*\\.hpp$")
            install(FILES ${source} DESTINATION include/crossbow/infinityverbs)
        endif()
    endforeach()
else()
    message(STATUS "Did not find Ibverns, Infinityverbs will not be built")
endif()

