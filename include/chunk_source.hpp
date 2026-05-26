#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace maxxcast {

// Metadata about the file being distributed. Sent to every client as part of the handshake
struct FileMeta {
    std::string filename;
    uint64_t    total_size;
    uint32_t    chunk_size;
    uint64_t    chunk_count;
    std::string sha256_hex;
};

struct ChunkView {
    const uint8_t* data; //pointer to the mapped file view
    size_t         len; //byte count
    uint64_t       index; //0 based indexing for the chunk
};

class ChunkSource {
public:
    virtual ~ChunkSource() = default;

    virtual const FileMeta& metadata() const = 0;

    // The returned pointer is valid for the lifetime of this ChunkSource.
    virtual ChunkView get_chunk(uint64_t index) const = 0;

    uint64_t chunk_count() const { return metadata().chunk_count; }
};

}
