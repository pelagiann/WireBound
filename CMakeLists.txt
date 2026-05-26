cmake_minimum_required(VERSION 3.20)
project(MaxxCast VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(mmap_proof tests/mmap_proof.cpp)
target_include_directories(mmap_proof PRIVATE include third_party)
if(WIN32)
    target_link_libraries(mmap_proof PRIVATE ws2_32)
endif()

add_library(chunk_source STATIC
    src/server/mapped_chunk_source.cpp
)
target_include_directories(chunk_source PUBLIC include third_party)
if(WIN32)
    target_link_libraries(chunk_source PRIVATE ws2_32)
endif()

add_executable(chunk_source_test tests/chunk_source_test.cpp)
target_include_directories(chunk_source_test PRIVATE include third_party)
target_link_libraries(chunk_source_test PRIVATE chunk_source)
if(WIN32)
    target_link_libraries(chunk_source_test PRIVATE ws2_32)
endif()

add_executable(Server src/Server.cpp)
target_include_directories(Server PRIVATE include third_party)
if(WIN32)
    target_link_libraries(Server PRIVATE chunk_source ws2_32)
endif()

add_executable(Client src/Client.cpp)
target_include_directories(Client PRIVATE include third_party)
if(WIN32)
    target_link_libraries(Client PRIVATE ws2_32)
endif()
