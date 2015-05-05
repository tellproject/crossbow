find_package(Boost 1.49 COMPONENTS context)
if (${Boost_FOUND})
    include_directories(${Boost_INCLUDE_DIR})
    set(CMAKE_THREAD_PREFER_PTHREAD ON)
    find_package(Threads REQUIRED)

    set(SRC
        thread/thread.hpp
        thread/mutex.hpp
        thread.cpp
        )

    add_library(crossbow_thread ${SRC})
    target_link_libraries(crossbow_thread ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})

    install(FILES "thread.hpp" DESTINATION include/crossbow)
    install(DIRECTORY "thread" DESTINATION include/crossbow)
    install(TARGETS crossbow_thread DESTINATION lib)

    enable_testing()

    macro(do_test exec)
        add_executable(${exec} ${exec}.cpp)
        target_link_libraries(${exec} crossbow_thread ${Boost_LIBRARIES})
        add_test(${exec}_test ${exec})
    endmacro(do_test)

    do_test(simple_test)
    do_test(fibonacci)
    set(HAS_THREAD True)
else()
    message(STATUS "Could not find Boost - won't build thread library")
    set(HAS_THREAD False)
endif()
