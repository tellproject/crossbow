set(SRCS
    include/crossbow/allocator.hpp
    src/allocator.cpp
    include/crossbow/ChunkAllocator.hpp
    src/ChunkAllocator.cpp
)

# Add the Allocator library
add_library(crossbow_allocator STATIC ${SRCS})
target_include_directories(crossbow_allocator PUBLIC
        $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# Link against Crossbow
target_include_directories(crossbow_allocator PRIVATE ${Crossbow_INCLUDE_DIRS})

# Install the library
install(TARGETS crossbow_allocator
        EXPORT CrossbowAllocatorTargets
        ARCHIVE DESTINATION ${LIB_INSTALL_DIR})

# Install Crossbow Allocator headers
install(DIRECTORY include/crossbow DESTINATION ${INCLUDE_INSTALL_DIR} FILES_MATCHING PATTERN "*.hpp")

# Install the cmake target
install(EXPORT CrossbowAllocatorTargets DESTINATION ${CMAKE_INSTALL_DIR})

# Documentation
set(CROSSBOW_LIB_PUBLIC_HDRS "${CROSSBOW_LIB_PUBLIC_HDRS} ${CMAKE_CURRENT_SOURCE_DIR}/include/crossbow" PARENT_SCOPE)
