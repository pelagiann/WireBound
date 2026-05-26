#pragma once

#include <cstdint>

struct ChunkHeader
{
    uint64_t index;
    uint32_t size;
};

struct FileMetaWire
{
    uint64_t total_size;
    uint32_t chunk_size;
    uint64_t chunk_count;
    char     sha256_hex[64];
};
