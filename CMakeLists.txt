cmake_minimum_required(VERSION 3.21)
project(GotitUpdater)

set(CMAKE_CXX_STANDARD 14)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

include_directories(${CURL_INCLUDE_DIR})

add_executable(GotitUpdater main.cpp DownloadModule/download.cpp DownloadModule/download.h)

target_link_libraries(GotitUpdater ${CURL_LIBRARY})
target_link_libraries (GotitUpdater ${CMAKE_THREAD_LIBS_INIT})

#add_compile_options(-fsanitize=address)
#add_compile_options( -fno-omit-frame-pointer)