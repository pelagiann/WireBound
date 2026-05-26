cmake_minimum_required(VERSION 3.20)
project(WireBound VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# mmap proof (throwaway — proves MapViewOfFile works)
add_executable(mmap_proof tests/mmap_proof.cpp)
target_include_directories(mmap_proof PRIVATE include third_party)
if(WIN32)
    target_link_libraries(mmap_proof PRIVATE ws2_32)
endif()

# MappedChunkSource static library (data plane core)
add_library(chunk_source STATIC
    src/server/mapped_chunk_source.cpp
)
target_include_directories(chunk_source PUBLIC include third_party)
if(WIN32)
    target_link_libraries(chunk_source PRIVATE ws2_32)
endif()

# chunk_source_test (verifies chunking + SHA-256)
add_executable(chunk_source_test tests/chunk_source_test.cpp)
target_include_directories(chunk_source_test PRIVATE include third_party)
target_link_libraries(chunk_source_test PRIVATE chunk_source)
if(WIN32)
    target_link_libraries(chunk_source_test PRIVATE ws2_32)
endif()

# Server
add_executable(Server src/Server.cpp)
target_include_directories(Server PRIVATE include)
if(WIN32)
    target_link_libraries(Server PRIVATE ws2_32)
endif()

# Client
add_executable(Client src/Client.cpp)
target_include_directories(Client PRIVATE include)
if(WIN32)
    target_link_libraries(Client PRIVATE ws2_32)
endif()
