#pragma once

#include "chunk_source.hpp"
#include <windows.h>
#include <string>

namespace maxxcast {

class MappedChunkSource : public ChunkSource {
public:
    // Opens and maps the file. Computes SHA-256 over the full content.
    explicit MappedChunkSource(const std::string& filepath,
                             uint32_t chunk_size = 1024 * 1024);

    // Unmaps the view and closes all handles.
    ~MappedChunkSource() override;

    // Non-copyable, non-movable: owns Windows HANDLEs
    MappedChunkSource(const MappedChunkSource&)            = delete;
    MappedChunkSource& operator=(const MappedChunkSource&) = delete;

    const FileMeta& metadata()                    const override;
    ChunkView       get_chunk(uint64_t index)     const override;

private:
    FileMeta       meta_;

    HANDLE         hFile_ = INVALID_HANDLE_VALUE;
    HANDLE         hMap_  = nullptr;
    const uint8_t* view_  = nullptr;
};

}
