cmake_minimum_required(VERSION 2.8)
project(crossbow)

if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -stdlib=libc++ -fPIC -Werror -march=native -ferror-limit=5")
else(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -fPIC -Werror -fmax-errors=5 -march=native")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -ggdb")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb")
endif(APPLE)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

enable_testing()
add_subdirectory(test)

