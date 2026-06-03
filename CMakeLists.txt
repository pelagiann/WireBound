cmake_minimum_required(VERSION 3.20)
project(WireBound VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to an optimized build when no type is given.
# Without this, plain `cmake -B build` builds at -O0 and the software
# SHA-256 hashing runs many times slower.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# LZ4 lossless compression (C source, compiled as a small static lib)
add_library(lz4 STATIC third_party/lz4.c)
target_include_directories(lz4 PUBLIC third_party)

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
    target_link_libraries(Server PRIVATE chunk_source lz4 ws2_32)
    # Static runtime so the .exe is self-contained and portable to other PCs.
    target_link_options(Server PRIVATE -static -static-libgcc -static-libstdc++)
endif()

add_executable(Client src/Client.cpp)
target_include_directories(Client PRIVATE include third_party)
if(WIN32)
    target_link_libraries(Client PRIVATE lz4 ws2_32)
    target_link_options(Client PRIVATE -static -static-libgcc -static-libstdc++)
endif()
