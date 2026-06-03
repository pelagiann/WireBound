#pragma once

#include <cstdint>


enum PacketType : uint32_t
{
    PACKET_CHUNK = 1,
    PACKET_END   = 2
};


// Per-chunk frame header.
// orig_size = uncompressed length; wire_size = bytes actually sent.
// compressed = 1 when the payload is LZ4-compressed, 0 when stored raw.
struct ChunkHeader
{
    uint32_t type;
    uint64_t index;
    uint32_t orig_size;
    uint32_t wire_size;
    uint32_t compressed;
};

struct FileMetaWire
{
    uint64_t total_size;
    uint32_t chunk_size;
    uint64_t chunk_count;
    char     sha256_hex[64];
    uint32_t compression;   // 0 = none, 1 = LZ4
};


