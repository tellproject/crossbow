set(CMAKE_THREAD_PREFER_PTHREAD ON)
find_package(Threads)

find_package(Boost 1.49 COMPONENTS context)
if (NOT ${Boost_FOUND})
    message(STATUS "Did not find Boost Context, thread will not be built")
    return()
endif()

set(STACK_SIZE "0x800000" CACHE STRING "The size of a stack")
configure_file(config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.hpp)

set(SRC
    thread/thread.hpp
    thread/mutex.hpp
    thread/condition_variable.hpp
    thread.cpp
    mutex.cpp
    common.hpp
)

# Add the Thread library
add_library(crossbow_thread STATIC ${SRC})
target_include_directories(crossbow_thread PUBLIC
        $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>)
target_include_directories(crossbow_thread PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Link against Threads
target_link_libraries(crossbow_thread PUBLIC ${CMAKE_THREAD_LIBS_INIT})

# Link against Boost
target_include_directories(crossbow_thread PUBLIC ${Boost_INCLUDE_DIRS})
target_link_libraries(crossbow_thread PUBLIC ${Boost_LIBRARIES})

# Install the library
install(TARGETS crossbow_thread
        EXPORT CrossbowThreadTargets
        ARCHIVE DESTINATION ${LIB_INSTALL_DIR})

# Add the thread unit tests
macro(do_test exec)
    add_executable(${exec} ${exec}.cpp)
    target_link_libraries(${exec} crossbow_thread)
    add_test("${exec}_test" ${exec})
endmacro(do_test)

if(${ENABLE_TESTS})
    enable_testing()
    do_test(simple_test)
    do_test(fibonacci)
endif()

# Install Crossbow Thread headers
install(DIRECTORY thread DESTINATION ${INCLUDE_INSTALL_DIR}/crossbow FILES_MATCHING PATTERN "*.hpp")
install(FILES "thread.hpp" DESTINATION ${INCLUDE_INSTALL_DIR}/crossbow)

# Install the cmake target
install(EXPORT CrossbowThreadTargets DESTINATION ${CMAKE_INSTALL_DIR})
